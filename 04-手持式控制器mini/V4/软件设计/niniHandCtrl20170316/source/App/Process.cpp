/**
  ******************************************************************************
	*文件：Process.cpp
	*作者：吴新有
	*版本：1.0
	*日期：2016-12-17
	*概要：
	*备注：
	*
  ******************************************************************************  
	*/ 
/* 头文件包含 ------------------------------------------------------------------*/
#include "Process.h"
#include "bmp.h"

#ifdef __cplusplus
 extern "C" {
#endif
	 
#include <stdlib.h>
#ifdef __cplusplus
 }
#endif
 
/* 类的实现--------------------------------------------------------------------*/
 	const u8 Coordinate_X[15]={/*0,8,16,32,40,*/48,56,64,72,80,88,96,104,112,120};  //液晶屏X 坐标数组
 	 u8 pinPointControl[]="Pin Point";
	 u8 approachControl[]="Approach ";
	 u8 shock[]="Shock";
	 u8 alarm[]="Alarm";
	 u8 noAction[]="    ";
 
 
/**
  * @brief  构造函数
  * @param  None
  * @retval None
  */
 
 
  Process::Process()
{
	enable_SWD_disable_Jtag();											//	关闭JTAG打开SWD	
	initGPIO();															//	初始化指示灯
	
	Pow_EN->setOn();	//开机后立即使能电源
	YL_EN->setOn();   //使能数传模块
	YL_Set->setOn();
	
	SysTick_DelayMs(100);   //延时等待液晶屏上电稳定
	
	initADs();															//	初始化AD通道
	initUsarts();														//	初始化串口
	initTimers();														//	初始化定时器	
	initOLED();
	initAllData();														//	初始化所有数据
	
	//初始化操作

	Led->setOff();													//	



	//显示：系统名称
	showTitleName();
	showControlMode();

//显示电量
	getPowerValue();
	showPowerValue();
	
	

}
/**
  * @brief  TIM2定时器中断函数,用于接收和处理按键
  * @param  None
  * @retval None
	* @周期 50ms
  */
void Process::runOnTime2(void)
{
	static int index=0;
	if(index>100)
	{
		
		index=0;
	}
	index++;
	

	getKeyValue();													//	获取AD按键值
	dealKeyValue();														  //	处理按键

	//如果有操作按键按下，更新显示
	if( returnKeyEvent() )
	{
		showControlStatus();
		showControlMode();
	}

	//如果没有按键按下，复位控制数据
	if(index%10==0)
	if( !returnKeyValue() )
	{
			controlStatusFlag=0;
	}
	
	

}

/**
  * @brief  TIM3定时器中断函数,80ms
  * @param  None
  * @retval None
	*定时：80ms
  */
void Process::runOnTime3(void)
{	
	if(controlStatusFlag)
	{
		updataToZigbee();
	}
	
	
	showOtherSymbol();
}

/**
  * @brief  TIM4定时器中断函数，用于刷新显示控制状态，200ms
  * @param  None
  * @retval None
  */

void Process::runOnTime4(void)
{
	//static int sleepTimer=0;
	
	showControlStatus();

	//获取电量值
	getPowerValue();
	showPowerValue();

//定时关断POW_EN
	watchDogTimer++;
	if(watchDogTimer>100)
	{
		watchDogTimer=0;
		Pow_EN->setOff();
	}
	
	
#ifdef USE_SLEEP
	//模拟看门狗休眠
	watchDogTimer++;
	if(watchDogTimer>200)
	{
		watchDogTimer=0;
		OLED_Display_Off();//关闭液晶屏显示
		SysTick_DelayMs(100);
	//	PWR_EnterSTANDBYMode();  //进入待机
		
	
		PWR_EnterSTOPMode(PWR_Regulator_ON,PWR_STOPEntry_WFI);   //进入停机
		//SysTick_DelayMs(10);
	//	SYSCLKconfig_STOP();		
	}
	else if(watchDogTimer<2)
	{
		OLED_Display_On();
	}
	
#endif
	
}



/**
  * @brief  初始化IO
  * @param  None
  * @retval None
  */
void Process::initGPIO()
{	

	//按键实例化
	keyNum1    = new Key(KEYNUM1_PORT,KEYNUM1_PIN);    //数字按键1
	keyNum2    = new Key(KEYNUM2_PORT,KEYNUM2_PIN);     //数字按键2
	keyNum3    = new Key(KEYNUM3_PORT,KEYNUM3_PIN);    //数字按键3
	keyNum4    = new Key(KEYNUM4_PORT,KEYNUM4_PIN);    //数字按键4
	keyNum5    = new Key(KEYNUM5_PORT,KEYNUM5_PIN);    //数字按键5
	keyNum6    = new Key(KEYNUM6_PORT,KEYNUM6_PIN);    //数字按键6
	keyNum7    = new Key(KEYNUM7_PORT,KEYNUM7_PIN);    //数字按键7
	keyNum8    = new Key(KEYNUM8_PORT,KEYNUM8_PIN);    //数字按键8
	keyNum9    = new Key(KEYNUM9_PORT,KEYNUM9_PIN);    //数字按键9
	keyNum0    = new Key(KEYNUM0_PORT,KEYNUM0_PIN);    //数字按键0

	
	keyDel     = new Key(KEYDEL_PORT,KEYDEL_PIN);      //退格键
	keyMode    = new Key(KEYMODE_PORT,KEYMODE_PIN);   //模式按键
	keyShock   = new Key(KEYSHOCK_PORT,KEYSHOCK_PIN);
	keyAlarm   = new Key(KEYALARM_PORT,KEYALARM_PIN);
	
	Led        = new IoOut(LED_PORT,LED_PIN);
	//电源使能
	Pow_EN = new IoOut(POW_EN_PORT,POW_EN_PIN,true);
	//数传使能
	YL_EN  = new IoOut(YL_EN_PORT,YL_EN_PIN,true);
	YL_Set = new IoOut(YL_SET_PORT,YL_SET_PIN,true);
	

	

}


/**
  * @brief  初始化AD
  * @param  None
  * @retval None
  */
void Process::initADs()
{
	ADC1_DMA_Init();
}

/**
  * @brief  初始化定时器
  * @param  None
  * @retval None
  */
void Process::initTimers()
{
	t2=new Timer (TIM2);
	t2->setPriority(STM32_NVIC_TIM2_PrePriority,STM32_NVIC_TIM2_SubPriority);
	t2->setTimeOut(TIM2_DELAY_TIME);		// 修改宏定义以改变延时

	t3=new Timer (TIM3);
	t3->setPriority(STM32_NVIC_TIM2_PrePriority,STM32_NVIC_TIM3_SubPriority);
	t3->setTimeOut(TIM3_DELAY_TIME);		// 修改宏定义以改变延时
	
	t4=new Timer (TIM4);
	t4->setPriority(STM32_NVIC_TIM2_PrePriority,STM32_NVIC_TIM4_SubPriority);
	t4->setTimeOut(TIM4_DELAY_TIME);		// 修改宏定义以改变延时
}

/**
  * @brief  初始化usart
  * @param  None
  * @retval None
  */
void Process::initUsarts()
{

	zigbeePort = new SerialPort(1,9600);	
	zigbeePort->open();

	zigbeeControl = new TerminalControl(zigbeePort);
	
	//lcdPort=new SpiPort(1);
//	lcdPort->open();
	
	//LCDCtrl=new LCDdisplay(lcdPort);															//控制液晶屏
  
}
/**
  * @brief  初始化液晶屏
  * @param  None
  * @retval None
  * @author 吴新有
  */
void Process::initOLED()
{

		OLED_Init();			//初始化OLED  
		OLED_Clear(); 
	

}

/**
  * @brief  初始化全部数据
  * @param  None
  * @retval None
  */	
void Process::initAllData(void)
{
	
	OLEDString=new u8[20];
	for(int i=0;i<20;i++)
	{
		OLEDString[i]=i+48;
		
	}
	
	sendData =new u8[6];
	for(int i=0;i<6;i++)
	{
		sendData[i]=0;
	}
	controlData=0;
	
	sqStack=new SqStack();
	
	controlModeFlag=1;           //默认是接近控制模式
	controlStatusFlag=0;         //默认是什么操作都没有
	isFreeMode=1;
	isShocking=false;
	isAlarming=false;

	watchDogTimer=0;
	powerValue=0;

	
}


	 
/**
  * @brief  开启用户外设
  * @param  None
  * @retval None
  */
void Process::openPeriph()
{
	SysTick_DelayMs(10);												//	延时 10ms

	t2->start();														//	开启定时器2
	t3->start();														//	开启定时器3
	t4->start();														//	开启定时器4
}

//////////////////////////////////////////////end of Init/////////////////////////////////////////////////////////


/**
  * @brief  计算电源电压
  * @param  None
  * @retval None
  */
void Process::getPowerValue(void)
{


	powerValue  = AD_Filter[NUM_POWER]*33*3>>12;


}


/**
  * @brief  获取按键值
  * @param  None
  * @retval None
  * @author 吴新有
  */
void Process::getKeyValue()
{

	
	keyNum1->getValue(); //1
	keyNum2->getValue(); //2
	keyNum3->getValue();
	keyNum4->getValue();
	keyNum5->getValue();
	keyNum6->getValue();
	keyNum7->getValue();
	keyNum8->getValue();
	keyNum9->getValue();
	keyNum0->getValue(); //0
	
	keyDel->getValue();
	keyMode->getValue();
	keyShock->getValue();
	keyAlarm->getValue();
	

	
	
}
/**
  * @brief  返回是否有按键按下
  * @param  None
  * @retval None	
	* 
  */
bool Process::returnKeyEvent()
{
	static bool preKeyStatus[3] = {true};
	
	if(preKeyStatus[0] != keyAlarm->FinalState)
	{
		preKeyStatus[0] = keyAlarm->FinalState;
		return true;
	}
	
	if(preKeyStatus[1] != keyShock->FinalState)
	{
		preKeyStatus[1] = keyShock->FinalState;
		return true;
	}
	
	if(preKeyStatus[2] != keyMode->FinalState)
	{
		preKeyStatus[2] = keyMode->FinalState;
		return true;
	}
	
	return false;
}

/**
  * @brief  返回所有按键的值的 或
  * @param  None
  * @retval None	
	* 
  */
bool Process::returnKeyValue()
{
	
	if(!keyMode->FinalState || !keyAlarm->FinalState || !keyShock->FinalState || !keyDel->FinalState
		 || !keyNum1->FinalState || !keyNum2->FinalState || !keyNum3->FinalState || !keyNum4->FinalState
		 || !keyNum5->FinalState || !keyNum6->FinalState || !keyNum7->FinalState || !keyNum8->FinalState
	   || !keyNum9->FinalState || !keyNum0->FinalState)
	{
		
		return true;
	}
	
	return false;
}
/**
  * @brief  获取控制数据
  * @param  None
  * @retval None	
  */
#define	POWER_OFF_COUNT_DOWN			10							//	关机计时,5s以后自动关机
#define	POWER_OFF_HARD_WARE_COUNT_DOWN	50							//	强制关机计时
void Process::dealKeyValue()
{
 	static 	bool	preNumdKey[14]		={true};

//如果是定点控制，则使能数字按键
if(0==controlModeFlag)
{
		
	//按一下Num1键
 	if (preNumdKey[1] != keyNum1->FinalState)					//	
 	{
 		preNumdKey[1]=keyNum1->FinalState;
		if (keyNum1->FinalState==false)
		{
			if(!sqStack->IsFull())
			{
				sqStack->Push('1');			
			
				OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
			}
			Led->reverseState();
			
		}

 	}
	
		//按一下Num2键
 	if (preNumdKey[2] != keyNum2->FinalState)					//	
 	{
 		preNumdKey[2]=keyNum2->FinalState;
		if (keyNum2->FinalState==false)
		{
			
			if(!sqStack->IsFull())
			{
			sqStack->Push('2');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
	
			//按一下Num3键
 	if (preNumdKey[3] != keyNum3->FinalState)					//	
 	{
 		preNumdKey[3]=keyNum3->FinalState;
		if (keyNum3->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('3');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
				//按一下Num4键
 	if (preNumdKey[4] != keyNum4->FinalState)					//	
 	{
 		preNumdKey[4]=keyNum4->FinalState;
		if (keyNum4->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('4');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
					//按一下Num5键
 	if (preNumdKey[5] != keyNum5->FinalState)					//	
 	{
 		preNumdKey[5]=keyNum5->FinalState;
		if (keyNum5->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('5');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
	
					//按一下Num6键
 	if (preNumdKey[6] != keyNum6->FinalState)					//	
 	{
 		preNumdKey[6]=keyNum6->FinalState;
		if (keyNum6->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('6');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
	
					//按一下Num7键
 	if (preNumdKey[7] != keyNum7->FinalState)					//	
 	{
 		preNumdKey[7]=keyNum7->FinalState;
		if (keyNum7->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('7');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
	
					//按一下Num8键
 	if (preNumdKey[8] != keyNum8->FinalState)					//	
 	{
 		preNumdKey[8]=keyNum8->FinalState;
		if (keyNum8->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('8');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
	
					//按一下Num9键
 	if (preNumdKey[9] != keyNum9->FinalState)					//	
 	{
 		preNumdKey[9]=keyNum9->FinalState;
		if (keyNum9->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('9');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
					//按一下Num0键
 	if (preNumdKey[0] != keyNum0->FinalState)					//	
 	{
 		preNumdKey[0]=keyNum0->FinalState;
		if (keyNum0->FinalState==false)
		{
			
			if(!sqStack->IsFull())       //非满
			{
			sqStack->Push('0');
			OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,sqStack->GetTopElem());
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],sqStack->GetTopElem() );
				
			}

			Led->reverseState();
			
		}

 	}
}
		//按一下DEL键
 	if (preNumdKey[10] != keyDel->FinalState)					//	
 	{
 		preNumdKey[10]=keyDel->FinalState;
		if (keyDel->FinalState==false)
		{
			if(!sqStack->IsEmpty())                                       //非空
			{
				OLED_ShowChar(Coordinate_X[sqStack->GetTop()],4,' ');	//显示空格，代替清除	
			//	LCDCtrl->writeLargeChar(4,Coordinate_X[sqStack->GetTop()],' ' );				
				sqStack->Pop();

			}
			
		}

 	}
	
			//按一下MODE键
 	if (preNumdKey[11] != keyMode->FinalState)					//	
 	{
 		preNumdKey[11]=keyMode->FinalState;
		if (keyMode->FinalState==false)
		{
			controlModeFlag=1-controlModeFlag;         //实现1和0的切换
			
			showControlMode();     //
		
			//终止控制状态动作
			controlStatusFlag=0;

			
		}

 	}
	
	
				//按一下SHOCK键
 	if (preNumdKey[12] != keyShock->FinalState)					
 	{
 		preNumdKey[12]=keyShock->FinalState;
		if (keyShock->FinalState==false)
		{
			controlStatusFlag=2;         //电击

			showControlStatus();
			
		}

 	}
	
	
					//按一下ALARM键
 	if (preNumdKey[13] != keyAlarm->FinalState)					//	
 	{
 		preNumdKey[13]=keyAlarm->FinalState;
		if (keyAlarm->FinalState==false)
		{
			controlStatusFlag=1;         //警报

			showControlStatus();
			
		}

 	}
	
	//喂狗
	if(!keyMode->FinalState || !keyAlarm->FinalState || !keyShock->FinalState || !keyDel->FinalState
		 || !keyNum1->FinalState || !keyNum2->FinalState || !keyNum3->FinalState || !keyNum4->FinalState
		 || !keyNum5->FinalState || !keyNum6->FinalState || !keyNum7->FinalState || !keyNum8->FinalState
	   || !keyNum9->FinalState || !keyNum0->FinalState)
	{
		
		watchDogTimer=3;//最好大于3，避免多余的刷屏显示
	}


}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////LCD显示////////////////////////////////////////////////////////

/**
  * @brief  显示OLED字符数据
  * @param  None
  * @retval None
  * @author:吴新有
  */
void Process::showOtherSymbol()
{


	//显示中间横线
	if(sqStack->GetTop()>2)
	{
	//	OLED_ShowChar(24,0,'-',16);		//显示横线
	}
	else
	{
	//	OLED_ShowChar(24,0,' ',16);		//显示空格，代替清除
	}
	
	//显示电量图标
	
}

/**
  * @brief  显示OLED电量图标
  * @param  None
  * @retval None
  * @author:吴新有
  */
void Process::showPowerValue()
{
	//显示电量图标
	u8 str100[5]="100%";
	u8 str80[5] =" 80%";
	u8 str60[5] =" 60%";
	u8 str40[5] =" 40%";
	u8 str20[5] =" 20%";
	u8 str5[5]  ="  5%";
	if(powerValue>=41)	
		OLED_ShowString(36,2,str100);	
	else if(powerValue>=40)
		OLED_ShowString(36,2,str80);	
	else if(powerValue>38)
		OLED_ShowString(36,2,str60);	
	else if(powerValue>37)
		OLED_ShowString(36,2,str40);	
	else if(powerValue>36)
		OLED_ShowString(36,2,str20);	
	else 
		OLED_ShowString(36,2,str5);	

	
}
/**
  * @brief  显示控制模式
  * @param  None
  * @retval None
  * @author:吴新有
  */
void Process::showControlMode()
{

	
	if(controlModeFlag)
	{
		//清空栈
		while( !sqStack->IsEmpty() )
		{
			sqStack->Pop();
		}
		
		//显示接近模式
		OLED_ShowEFootCHinese12X12(0,6,4);
		OLED_ShowEFootCHinese12X12(12,6,5);
		OLED_ShowEFootCHinese12X12(24,6,2);
		OLED_ShowEFootCHinese12X12(36,6,3);
		//在编号的地方显示空白
		u8 strBlank[]="             ";
		OLED_ShowString(0,4,strBlank);	
		
	//	LCDCtrl->writeHanziLine(6,4,"接近控制",4,true,0,0x00);
		//LCDCtrl->writeLargeCharLine(6,4,"Approach",8,true,false);
	//	LCDCtrl->writeLargeCharLine(4,4,"               ",15,true,false);
	}
	else
	{
		//显示定点模式
		OLED_ShowEFootCHinese12X12(0,6,0);
		OLED_ShowEFootCHinese12X12(12,6,1);
		OLED_ShowEFootCHinese12X12(24,6,2);
		OLED_ShowEFootCHinese12X12(36,6,3);
		
	//  LCDCtrl->writeHanziLine(6,4,"定点控制",4,true,0,0x00);
		//LCDCtrl->writeLargeCharLine(6,4,"Point   ",8,true,false);
		//显示输入编码：
		OLED_ShowEFootCHinese12X12(0,4,17);
		OLED_ShowEFootCHinese12X12(12,4,18);
		OLED_ShowEFootCHinese12X12(24,4,19);
	//	LCDCtrl->writeHanziLine(4,4,"编号：",3,true,false);
	}
		


}

/**
  * @brief  显示控制模式
  * @param  None
  * @retval None
  * @author:吴新有
  */
#define SHOCKTIME  15
#define ALARMTIME  15
void Process::showControlStatus()
{

	
	static int shockTimeCount=0,alarmTimeCount=0;
	if(2==controlStatusFlag)     //正在电击
	{

		//显示电击
		OLED_ShowEFootCHinese12X12(90,6,8);
		OLED_ShowEFootCHinese12X12(102,6,9);

	}
	else if(1==controlStatusFlag) //正在警报
	{

		 //显示警报
		OLED_ShowEFootCHinese12X12(90,6,6);
		OLED_ShowEFootCHinese12X12(102,6,7);

	}
	else
	{
	
		OLED_ShowString(90,6,noAction);  //没有动作，显示空白
	}


}


//开机欢迎界面
void Process::disPage_start()
{

}

void Process::showTitleName()
{

//打印标题“电子脚扣控制器”
	OLED_ShowEFootCHinese12X12(16,0,10);
	OLED_ShowEFootCHinese12X12(28,0,11);
	OLED_ShowEFootCHinese12X12(40,0,12);
	OLED_ShowEFootCHinese12X12(52,0,13);
	OLED_ShowEFootCHinese12X12(64,0,14);
	OLED_ShowEFootCHinese12X12(76,0,15);
	OLED_ShowEFootCHinese12X12(88,0,16);
//打印“电量”
	OLED_ShowEFootCHinese12X12(0,2,20);
	OLED_ShowEFootCHinese12X12(12,2,21);
	OLED_ShowEFootCHinese12X12(24,2,19);


}

////////////////////////////////////////////LCD显示完///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
  * @brief  更新发送数据
  * @param  None
  * @retval None
  * @author:吴新有
  */

void Process::updataToZigbee()
{
	//更新数据
	sendData[0]=(isFreeMode<<4) +(controlStatusFlag <<2)+controlModeFlag;
	
	//如果有控制动作时，则不更新本体ID号，避免在动作时，误改变了本体ID号，然后动作会随ID号转移。
	//所以在动作时，改变本体ID号是无效的。
	//if(0==controlStatusFlag)
	{
		for(int i=0;i<=sqStack->GetTop();i++)
		{
			sendData[i+1] = sqStack->st.data[i]-'0';
		}
		for(int i=sqStack->GetTop()+1;i<5;i++)
		{
			sendData[i+1] = 0;
		}
	}
	

	
	//发送数据
	zigbeeControl->updateSendBuf(sendData);
	zigbeeControl->sendcommand();
	
}



/**
  * @brief  使能SWD，关闭JTAG
  * @param  None
  * @retval None
  */
void Process::enable_SWD_disable_Jtag()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);		//	使能AFIO时钟
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);		//	关闭JTAG，引脚作为GPIO用
}






/**
  * @brief  调试代码，串口输出
  * @param  None
  * @retval None
  */
void Process::testWithUsart(void)
{
	uint8_t aa;
	aa=AD_Filter[1]>>4;													//	调试代码	

//	testLed->reverseState();
}


/*******************************************************************************
* Function Name  : 从停机模式下唤醒之后： 配置系统时钟允许HSE，和 pll 作为系统时钟。
* Description    : Inserts a delay time.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void  Process::SYSCLKconfig_STOP(void)
{
		ErrorStatus HSEStartUpStatus;
    RCC_HSEConfig(RCC_HSE_ON); /*HSES使能*/  
    HSEStartUpStatus = RCC_WaitForHSEStartUp(); /*等待*/
   if(HSEStartUpStatus == SUCCESS) 
   { 
      RCC_PLLCmd(ENABLE);/*使能*/
         while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY)== RESET); /*等待PLL有效*/      
			RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);/*将PLL作为系统时钟*/
			while(RCC_GetSYSCLKSource() != 0x08);/*等待*/
   } 
}
/*--------------------------------- End of Process.cpp -----------------------------*/
