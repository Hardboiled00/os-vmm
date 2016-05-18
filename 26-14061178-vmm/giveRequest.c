#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "vmm.h"

#define FIFO "myFIFO"
int main()
{
	unsigned long addr;
	unsigned char value;
	char s[100];
	srandom(time(NULL));
	FILE *fp;
	int number;
	int pid;
	
	while(1){
		pid=random()%2;
		if((fp=fopen(FIFO,"w"))==NULL){
			printf("write FIFO wrong");
			return 0;
		}
		/* 随机产生请求地址 */
		//ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
		scanf("%s",s);
	    addr=random() % VIRTUAL_MEMORY_SIZE;		
		/* 随机产生请求类型 */
		switch (random() % 3)
		{
			case 0: //读请求
			{
				//ptr_memAccReq->reqType = REQUEST_READ;
				number=0;
				printf("产生请求：\n地址：%lu\t类型：读取 pid:%d\n", addr,pid);
				fprintf(fp,"%d %lu %d",number,addr,pid);
				break;
			}
			case 1: //写请求
			{
				number=1;
				//ptr_memAccReq->reqType = REQUEST_WRITE;
				/* 随机产生待写入的值 */
				value = random() % 0xFFu;
				fprintf(fp,"%d %lu %02X %d",number,addr,value,pid);
				printf("产生请求：\n地址：%lu\t类型：写入\t值：%02X pid:%d\n",addr, value,pid);
				break;
			}
			case 2:
			{
				number=2;
				//ptr_memAccReq->reqType = REQUEST_EXECUTE;
				fprintf(fp,"%d %lu %d",number,addr,pid);
				printf("产生请求：\n地址：%lu\t类型：执行 pid:%d\n", addr,pid);
				break;
			}
			default:
				break;
		}	
		fclose(fp);
		sleep(5);
	}
	return 0;
}
