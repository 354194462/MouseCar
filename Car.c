#include <reg52.h>
#include "mouse.h"

sbit beep = P2 ^ 4;//找到蜂鸣器的地址位
bit ir = 0;//标志中断时发射还是接收
unsigned char num_G = 0;//标记这次发射红外光的LED管
const unsigned  int code forward[] = { 0x11,0x33,0x22,0x66,0x44,0xcc,0x88,0x99 };//电机正转周期
const unsigned  int code reverse[] = { 0x11,0x99,0x88,0xcc,0x44,0x66,0x22,0x33 };//电机反转周期
unsigned char map[8][8];//定义迷宫的8*8小方格 高四位记录进入迷宫小方格的方向 低四位记录迷宫小方格的挡板情况
unsigned char i, j;//循环体变量
unsigned char CarTurn_Dir = 0;//定义小车的转动方向
unsigned char CarReal_Dir = 0;//定义小车的绝对方向
unsigned char num_StackNow = 0;//记录小车在建立地图的等高表时所确定的队的目前位置
unsigned char num_Stack = 0;//记录栈中坐标的个数
unsigned char stackX[64], stackY[64];//使用数组模拟栈的效果，stackX记录岔路口的x坐标,stackY记录岔路口的y坐标
unsigned char mapFlag[8][8];//每个迷宫都有一个变量来记录迷宫方格的状态：0未走过 1走过
unsigned char dirNum = 0; //记录每个迷宫方格周围可以同行的方向的数量，来判断这个迷宫方格是否是岔路口

void InitMap();//迷宫方格的初始化函数
void InitMapFlag();//迷宫方格标记的初始化函数
void InitStack();//迷宫数组（栈、队列）的初始化
void InitTime2();//定时器T2初始化
void JudgeMap_byRed();//通过红外线检测迷宫方格周围的挡板情况，并记录到map[][]的低四位中
void JudgeMap_byDir();//通过小车的绝对方向来赋给下一个位置的通过迷宫小方格方向，并记录到map[][]的高四位中
void WorkForFork();//对没有走过的岔路口作出相应的处理
void SearchMode0();//右手法则
void SearchMode1();//左手法则
void SearchMode2();//中左法则
void SearchMode3();//中右法则
void DrawHeightMap();//画出地图的等高图
void MakeRushMap();//制作冲刺路线
void CarRush();//小车的最后冲刺
void WorkForEnd();//对小车处在死胡同或者终点（没有遍历完地图）做出相应的处理
char CarRegional();//判断小车所在的位置区域

void Delay_ms(int time);//延时函数
void Search_Mode(int data_car);//最终的小车的行走策略
void DriveCar(int Dir);//驱动小车函数
void PostureFix(int Dir);//小车的姿势修复
void GetReal_Dir(int Dir);//获取小车的绝对方向
void GetReal_Pos(int Dir);//获取小车的位置
void PlaySound(int time);//蜂鸣器发声
void SetTime2(unsigned int);//设置T2自动重载寄存器和计数器初值

struct mouseCar
{
	unsigned int x; 
	unsigned int y;
};//定义小车的结构体
struct mouseCar Car = {0,0};//定义小车的单例
//主函数
void main()
{
	InitTime2();
	InitMap();
	InitMapFlag();
	InitStack();
	//功能1：从指定起点开始在迷宫中行走，找到指定终点
	while (!(Car.x == 7 && Car.y == 7))
	{
		 /*
		 * 判断小车所在位置以及所在位置的的挡板情况
		 * 如果小车所在的位置是岔路口，则将岔路口压入栈
		 * 如果小车所在的位置是终点（未遍历完所有路线）或者死胡同，则小车将回到最近的岔路口从新开始行走
		 * 如果小车所在的位置既不是岔路口，也不是死胡同或者终点，那么直接按照寻路策略驱动小车行走
		 */
		
		if (dirNum > 1 && mapFlag[Car.x][Car.y] == 0)
			WorkForFork();
		else  if (dirNum == 0 || ((Car.x == 7 && Car.y == 7) && num_Stack != 0))
			WorkForEnd();
		mapFlag[Car.x][Car.y] = 1;//将当前小车所处在的迷宫方格标记为1
		DriveCar(CarTurn_Dir);//驱动小车行走
	}
	//功能2：小车从终点开始遍历迷宫返回起点，寻找最短路径
	InitMap();
	InitMapFlag();
	InitStack();
	num_Stack = 0;
	while (!(Car.x == 0 && Car.x == 0 && num_Stack == 0))
	{
		JudgeMap_byDir();
		JudgeMap_byRed();
		Search_Mode(CarRegional());//按照制订好的寻路策略来确定小车的行走方向
		GetReal_Dir(CarTurn_Dir);//获取小车的绝对行走方向
		GetReal_Pos(CarReal_Dir);//获取小车的绝对位置
		JudgeMap_byRed();
		if (dirNum > 1 && mapFlag[Car.x][Car.y] == 0)
			WorkForFork();
		else  if (dirNum == 0 || ((Car.x == 7 && Car.y == 7) && num_Stack != 0))
			WorkForEnd();
		mapFlag[Car.x][Car.y] = 1;//将当前小车所处在的迷宫方格标记为1
		DriveCar(CarTurn_Dir);//驱动小车行走
	}
	//功能3：根据最短路径冲刺到终点

	/*将地图的同一高度的地图方块进队，坐标存贮到队列中，等待下一次搜索时出队
	* 如果同一高度的节点全部进队，那么下次入队的地图方块高度数加一
	* 标记进队的地图方块，下次搜索到该方块时高度数不改变
	* 如果找到起点，那么终止搜索，画出等高表
	*/

	DrawHeightMap();

	/*
	* 利用等高表，从起点出发，每次找比上一个地图方块高度数小一的的地图方块
	* 记录冲刺的的每个地图方块的方向，记录到数组中
	* 小车冲刺到终点
	* 
	*/

	MakeRushMap();
	CarRush();


	//功能3：小车回到起点后蜂鸣器发出声音
	PlaySound(10);
	while (1);//无限循环，防止单片机跑飞
}
//延时函数
void Delay_ms(int time)
{
	for (i = time; i > 0; i--)
		for (j = 0; j < 100; j++);
}
//初始化地图
void InitMap()
{
	for(i = 0; i < 8; i ++)
		for (j = 0; j < 8; j++)
		{
			map[i][j] = 0xff;
		}
}
//初始化地图标记
void InitMapFlag()
{
	for(i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
		{
			mapFlag[i][j] = 0;
		}
}
//初始化栈、队列
void InitStack()
{
	for (i = 0; i < 64; i++)
	{
		stackX[i] = 0;
		stackY[i] = 0;
	}
}
//判断地图方格周围的挡板情况
void JudgeMap_byRed()
{
	if (!irC) { dirNum++; map[Car.x][Car.y] = map[Car.x][Car.y] & 0xf7; }//如果正前方没有挡板，那么挡板数加一，改变迷宫的低四位
	else if (!irR) { dirNum++; map[Car.x][Car.y] = map[Car.x][Car.y] & 0xfb; }//如果右前方没有挡板
	else if (!irL) { dirNum++; map[Car.x][Car.y] = map[Car.x][Car.y] & 0xfd; }//如果左前方没有挡板

	if (irLU && !irC)
		PostureFix(0);
	else if (irRU && !irC)
		PostureFix(1);

}
//判断地图方格的通过方向
void JudgeMap_byDir()
{
	switch (CarTurn_Dir)
	{
	case 0:map[Car.x][Car.y + 1] = map[Car.x][Car.y] & 0x7f;
	case 1:map[Car.x + 1][Car.y] = map[Car.x][Car.y] & 0xbf;
	case 2:map[Car.x][Car.y - 1] = map[Car.x][Car.y] & 0xdf;
	case 3:map[Car.x - 1][Car.y] = map[Car.x][Car.y] & 0xef;
	default:break;
	}

}
//获得小车的绝对方向
void GetReal_Dir(int Dir)
{
	CarReal_Dir = (CarReal_Dir + Dir) % 4;
}
//获得小车的位置
void GetReal_Pos(int Dir)
{
	switch (Dir)
	{
	case 0:Car.y += 1; break;
	case 1:Car.x += 1; break;
	case 2:Car.y -= 1; break;
	case 3:Car.x -= 1; break;
	default:break;
	}

}
//对没有走过的岔路口做出相应的处理
void WorkForFork()
{
	stackX[num_Stack] = Car.x;
	stackY[num_Stack] = Car.y;//将岔路口的坐标压入栈（数组）中
	num_Stack++;//栈的长度+1
}
//对死胡同做出相应的处理（未完成对回溯路径的处理）
void WorkForEnd()
{
	//回溯到最近的一个岔路口
	while (!(Car.x == stackX[num_Stack - 1] && Car.y == stackY[num_Stack - 1]))
	{
		switch (map[Car.x][Car.y] / 16)
		{
			//假如进来时朝前,则回去时朝后
		case 7:CarTurn_Dir = 2; break;
			//假如进来时朝右，则回去时朝左
		case 11:CarTurn_Dir = 3; break;
			//假如进来时朝后，则回去时朝前
		case 13:CarTurn_Dir = 0; break;
			//假如进来时朝左，则回去时朝右
		case 15:CarTurn_Dir = 1; break;
		}
		DriveCar(CarTurn_Dir);
	}
	num_Stack--;//栈的长度减一

}
//电机驱动函数
void DriveCar(int Dir)
{
	JudgeMap_byDir();//判断小车的绝对方向
	JudgeMap_byRed();//判断小车的所在位置的挡板情况
	Search_Mode(CarRegional());//按照制订好的寻路策略来确定小车的行走方向
	GetReal_Dir(CarTurn_Dir);//获取小车的绝对行走方向
	GetReal_Pos(CarReal_Dir);//获取小车的绝对位置
	JudgeMap_byRed();
	//判断小车的行走方向
	switch (Dir)
	{
		//如果向前走
	case 0: {
		for (j = 0; j < 110; j++)
		{
			for (i = 0; i < 8; i++)
			{
				P1 = forward[i];
				Delay_ms(3);
			}
		}
	}break;
		//如果向右走
	case 1: {
		for (j = 0; j < 55; j++)
		{
			for (i = 0; i < 8; i++)
			{
				P1 = (forward[i] | 0xf0);
				Delay_ms(3);
			}
		}
		for (j = 0; j < 55; j++)
		{
			for (i = 0; i < 8; i++)
			{
				P1 = (reverse[i] | 0x0f);
				Delay_ms(3);
					
			}
		}
	}
		  //如果向后走
	case 2: {
		for (j = 0; j < 110; j++)
		{
			for (i = 0; i < 8; i++)
			{
				P1 = reverse[i];
			}
		}
	}
		  //如果向左走
	case 3: {
		for (j = 0; j < 55; j++)
		{
			for (i = 0; i < 8; i++)
			{
				P1 = (reverse[i] | 0xf0);
				Delay_ms(3);
			}
		}
		for (j = 0; j < 55; j++)
		{
			for (i = 0; i < 8; i++)
			{
				P1 = (forward[i] | 0x0f);
				Delay_ms(3);

			}
		}
	}
	default:break;
	}
}
//小车的姿势修复
void PostureFix(int Dir)
{
	if (Dir == 0)//如果小车方向往左偏移，那么小车适当向右偏转
	{
		while (irLU)
			DriveCar(1);
	}
	else if(Dir == 1)//如果小车方向往右偏转，那么小车适当向左偏转
	{
		while (irRU)
			DriveCar(3);
	}
}
//右手法则
void SearchMode0()
{
	//如果可以向右转，则向右转
	if (map[Car.x][Car.y] | 0xfb == 0xfb)
	{
		CarTurn_Dir = 1;
		return;
	}

	//如果可以向前转，则向前转
	else if (map[Car.x][Car.y] | 0xf7 == 0xf7)
	{
		CarTurn_Dir = 0;
		return;
	}

	//如果可以向左转，则向左转
	else if (map[Car.x][Car.y] | 0xfe == 0xfe)
	{
		CarTurn_Dir = 3;
		return;
	}

}
//左手法则
void SearchMode1()
{
	//如果可以向左转，则向左转
	if (map[Car.x][Car.y] | 0xfe == 0xfe)
	{
		CarTurn_Dir = 3;
		return;
	}

	//如果可以向前转，则向前转
	else if (map[Car.x][Car.y] | 0xf7 == 0xf7)
	{
		CarTurn_Dir = 0;
		return;
	}

	//如果可以向右转，则向右转
	else if (map[Car.x][Car.y] | 0xfb == 0xfb)
	{
		CarTurn_Dir = 1;
		return;
	}

}
//中左法则
void SearchMode2()
{
	//如果可以向前转，则向前转
	if (map[Car.x][Car.y] | 0xf7 == 0xf7)
	{
		CarTurn_Dir = 0;
		return;
	}

	//如果可以向左转，则向左转
	else if (map[Car.x][Car.y] | 0xfe == 0xfe)
	{
		CarTurn_Dir = 3;
		return;
	}


	//如果可以向右转，则向右转
	else if (map[Car.x][Car.y] | 0xfb == 0xfb)
	{
		CarTurn_Dir = 1;
		return;
	}

}
//中右法则
void SearchMode3()
{
	//如果可以向前转，则向前转
	if (map[Car.x][Car.y] | 0xf7 == 0xf7)
	{
		CarTurn_Dir = 0;
		return;
	}

	//如果可以向右转，则向右转
	else if (map[Car.x][Car.y] | 0xfb == 0xfb)
	{
		CarTurn_Dir = 1;
		return;
	}

	//如果可以向左转，则向左转
	else if (map[Car.x][Car.y] | 0xfe == 0xfe)
	{
		CarTurn_Dir = 3;
		return;
	}
}
//判断小车所在的位置区域
char CarRegional()
{
	if (Car.x <= 3 && Car.y <= 3)
		return 0;
	else if (Car.x >= 3 && Car.y > 3)
		return 1;
	else if (Car.x <= 3 && Car.y > 3)
		return 2;
	else 
		return 3;
}
//最终的行走策略
void Search_Mode(int Data)
{
	switch (Data)
	{
	case 0: {
		if (CarReal_Dir == 0) SearchMode3();
		else if (CarReal_Dir == 1) SearchMode2();
		else if (CarReal_Dir == 2) SearchMode0();
		else if (CarReal_Dir == 3) SearchMode1();
	}break;
	case 1: {
		if (CarReal_Dir == 0) SearchMode2();
		else if (CarReal_Dir == 1) SearchMode1();
		else if (CarReal_Dir == 2) SearchMode3();
		else if (CarReal_Dir == 3) SearchMode0();
	}break;
	case 2: {
		if (CarReal_Dir == 0) SearchMode1();
		else if (CarReal_Dir == 1) SearchMode0();
		else if (CarReal_Dir == 2) SearchMode2();
		else if (CarReal_Dir == 3) SearchMode3();
	}break;
	case 3: {
		if (CarReal_Dir == 0) SearchMode0();
		else if (CarReal_Dir == 1) SearchMode3();
		else if (CarReal_Dir == 2) SearchMode1();
		else if (CarReal_Dir == 3) SearchMode2();
	}break;
	default:break;
	}

}
//蜂鸣器发出声音
void PlaySound(int time)
{
	while (time--)
	{
		beep = ~beep;
		Delay_ms(15);
	}
}
//画出地图的等高表
void DrawHeightMap()
{
	InitStack();
	InitMapFlag();
	num_StackNow = 0;//记录正在检测的队列的下标
	num_Stack = 0;//num_Stack记录队列的长度，并将坐标(0，0)放入队列
	stackX[0] = 7;
	stackY[0] = 7;
	mapFlag[0][0] = 1;//将起点的高度置为1
	while (num_StackNow <= num_Stack)
	{
		if (((map[stackX[num_StackNow]][stackY[num_StackNow]] % 16) | 0xf7 != 0xff) && (mapFlag[stackX[num_StackNow]][stackY[num_StackNow] + 1] != 0))//如果正在检测的地图方块上方没有挡板,且未进入过队列
		{
			num_Stack++;//将正在检测的地图方块的上方的方块放入队列
			stackX[num_Stack] = stackX[num_StackNow];
			stackY[num_Stack] = stackY[num_StackNow] + 1;
			mapFlag[stackX[num_Stack]][stackY[num_Stack]] = mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] + 1;//将这个地图方块的高度置为比正在检测的地图方块的高度+1

		}
		if (((map[stackX[num_StackNow]][stackY[num_StackNow]] % 16) | 0xfb != 0xff) && (mapFlag[stackX[num_StackNow] + 1][stackY[num_StackNow]] != 0))//如果正在检测的地图方块右方没有挡板
		{
			num_Stack++;//将正在检测的地图方块的右方的方块放入队列
			stackX[num_Stack] = stackX[num_StackNow] + 1;
			stackY[num_Stack] = stackY[num_StackNow];
			mapFlag[stackX[num_Stack]][stackY[num_Stack]] = mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] + 1;//将这个地图方块的高度置为比正在检测的地图方块的高度+1

		}
		if (((map[stackX[num_StackNow]][stackY[num_StackNow]] % 16) | 0xfd != 0xff) && (mapFlag[stackX[num_StackNow]][stackY[num_StackNow] - 1] != 0))//如果正在检测的地图方块后方没有挡板
		{
			num_Stack++;//将正在检测的地图方块的后方的方块放入队列
			stackX[num_Stack] = stackX[num_StackNow];
			stackY[num_Stack] = stackY[num_StackNow] - 1;
			mapFlag[stackX[num_Stack]][stackY[num_Stack]] = mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] + 1;//将这个地图方块的高度置为比正在检测的地图方块的高度+1

		}
		if (((map[stackX[num_StackNow]][stackY[num_StackNow]] % 16) | 0xfe != 0xff) && (mapFlag[stackX[num_StackNow] - 1][stackY[num_StackNow]] != 0))//如果正在检测的地图方块左方没有挡板
		{
			num_Stack++;//将正在检测的地图方块的左方的方块放入队列
			stackX[num_Stack] = stackX[num_StackNow] - 1;
			stackY[num_Stack] = stackY[num_StackNow];
			mapFlag[stackX[num_Stack]][stackY[num_Stack]] = mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] + 1;//将这个地图方块的高度置为比正在检测的地图方块的高度+1

		}
		num_StackNow++;
	}
}
//制作冲刺路线
void MakeRushMap()
{
	InitStack();
	num_StackNow = 0;//当前地图方块
	num_Stack = 0;//路线中的地图方块的个数
	stackX[0] = 0;
	stackY[0] = 0;
	//找到路径上的所有地图方块
	while (num_Stack < mapFlag[7][7])
	{
		//检测当前栈中的地图方块上方的地图方块的高度，并判断是否满足递减1的条件
		if (mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] - mapFlag[stackX[num_StackNow]][stackY[num_StackNow] + 1] == 1)
		{
			num_Stack++;
			stackX[num_Stack] = stackX[num_Stack - 1];
			stackY[num_Stack] = stackY[num_Stack - 1] + 1;
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] |= 0xf0;//给地图方块的高四位重新赋值
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] &= 0x70;
		}
		//检测当前栈中的地图方块右方的地图方块的高度，并判断是否满足递减1的条件
		else if (mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] - mapFlag[stackX[num_StackNow] + 1][stackY[num_StackNow]] == 1)
		{
			num_Stack++;
			stackX[num_Stack] = stackX[num_Stack - 1] + 1;
			stackY[num_Stack] = stackY[num_Stack - 1];
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] |= 0xf0;//给地图方块的高四位重新赋值
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] &= 0xb0;
		}
		//检测当前栈中的地图方块下方的地图方块的高度，并判断是否满足递减1的条件
		else if (mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] - mapFlag[stackX[num_StackNow]][stackY[num_StackNow] - 1] == 1)
		{
			num_Stack++;
			stackX[num_Stack] = stackX[num_Stack - 1];
			stackY[num_Stack] = stackY[num_Stack - 1] - 1;
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] |= 0xf0;//给地图方块的高四位重新赋值
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] &= 0xd0;
		}
		//检测当前栈中的地图方块左方的地图方块的高度，并判断是否满足递减1的条件
		else if (mapFlag[stackX[num_StackNow]][stackY[num_StackNow]] - mapFlag[stackX[num_StackNow] - 1][stackY[num_StackNow]] == 1)
		{
			num_Stack++;
			stackX[num_Stack] = stackX[num_Stack - 1] - 1;
			stackY[num_Stack] = stackY[num_Stack - 1];
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] |= 0xf0;//给地图方块的高四位重新赋值
			map[stackX[num_StackNow]][stackY[num_StackNow] + 1] &= 0xe0;
		}
	}
}
//小车的最后冲刺
void CarRush()
{
	while (!(Car.x == 0 && Car.y == 0))
	{
		switch (map[Car.x][Car.y] / 16)
		{
			//假如进来时朝前,则回去时朝后
		case 7:CarTurn_Dir = 2; break;
			//假如进来时朝右，则回去时朝左
		case 11:CarTurn_Dir = 3; break;
			//假如进来时朝后，则回去时朝前
		case 13:CarTurn_Dir = 0; break;
			//假如进来时朝左，则回去时朝右
		case 15:CarTurn_Dir = 1; break;
		}
		DriveCar(CarTurn_Dir);
	}
}
//设置T2自动重载寄存器和计数器初值
void SetTime2(unsigned int us)
{
	TH2 = (65536 - us) / 256;
	RCAP2H = (65536 - us) / 256;
	TL2 = (65536 - us) % 256;
	RCAP2L = (65536 - us) % 256;
}
//定时器T2初始化
void InitTime2()
{
	EA = 1;//总中断允许
	ET2 = 1;
	SetTime2(5000);
	TR2 = 1;
}
//T2中断服务函数
void time2() interrupt 5
{
	ET2 = 0;
	SetTime2(5000);
	TF2 = 0;//清除T2中断标志位
	if (!ir)//设置num_G方向发光二极管发射红外光
		MOUSE_IR_ON(num_G);
	else//检测num_G方向接收管返回的电平
	{
		switch (num_G)
		{
		case 0: {if (irR1) irC = 0; else irC = 1; }break;
		case 1: {if (irR2) irLU = 0; else irLU = 1; }break;
		case 2: {if (irR3) irL = 0; else irL = 1; }break;
		case 3: {if (irR4) irR = 0; else irR = 1; }break;
		case 4: {if (irR5) irRU = 0; else irRU = 1; }break;
		}
	}
	if (ir)//如果完成了一次发射和接收，那么开始检测下一个LED管
		num_G++;
	if (num_G == 5)//反复低检测5个红外LED灯管
		num_G = 0;
	ir = ~ir;//改变接收、发射状态
	ET2 = 1;
}