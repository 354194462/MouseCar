
sfr P4 = 0xe8;
sbit A0 = P4 ^ 0;//红外传感接受信号口定义
sbit A1 = P2 ^ 0;
sbit A2 = P2 ^ 7;
sbit irR1 = P2 ^ 1;//前
sbit irR2 = P2 ^ 2;//左外
sbit irR3 = P2 ^ 3;//左
sbit irR4 = P2 ^ 4;//右
sbit irR5 = P2 ^ 5;//右外
bit irC = 0, irL = 0, irR = 0, irLU = 0, irRU = 0;//定义红外传感器检测状态全局位变量，为 0 无障碍
//红外发射控制宏定义(传入传感器组号)
#define MOUSE_IR_ON(GROUP_NO) \
do\
{ \
A0=(GROUP_NO)&0x01; \
A1=(GROUP_NO)&0x02; \
A2=(GROUP_NO)&0x04; \
}while(0)