#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "vmm.h"
#define number_process 4
//#define DEBUG
/* ҳ�� */
FirstLevelPageTable pageTable[number_process];
/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE * number_process];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq;

PageRecord process_page[number_process][PAGE_SUM * number_process];
int page_number_process[number_process] = {};
int LRU_NUM=0;
int block_auxAddr[BLOCK_SUM];

/* ��ʼ������ */
void do_init()
{

	int i, j, k;
	srandom(time(NULL));
	for(k=0;k<32;k++)
	{
		block_auxAddr[k]=-1;
	}
	for(k = 0; k < number_process; k++){
		for (i = 0; i < PAGE_SUM; i++)
		{
			pageTable[k].SecondPageTable[i].pageNum = i;
			pageTable[k].SecondPageTable[i].filled = FALSE;
			pageTable[k].SecondPageTable[i].edited = FALSE;
			pageTable[k].SecondPageTable[i].count = LRU_NUM;
			/* ʹ����������ø�ҳ�ı������� */
			switch (random() % 7)
			{
				case 0:
				{
					pageTable[k].SecondPageTable[i].proType = READABLE;
					break;
				}
				case 1:
				{
					pageTable[k].SecondPageTable[i].proType = WRITABLE;
					break;
				}
				case 2:
				{
					pageTable[k].SecondPageTable[i].proType = EXECUTABLE;
					break;
				}
				case 3:
				{
					pageTable[k].SecondPageTable[i].proType = READABLE | WRITABLE;
					break;
				}
				case 4:
				{
					pageTable[k].SecondPageTable[i].proType = READABLE | EXECUTABLE;
					break;
				}
				case 5:
				{
					pageTable[k].SecondPageTable[i].proType = WRITABLE | EXECUTABLE;
					break;
				}
				case 6:
				{
					pageTable[k].SecondPageTable[i].proType = READABLE | WRITABLE | EXECUTABLE;
					break;
				}
				default:
					break;
			}
			/* ���ø�ҳ��Ӧ�ĸ����ַ */
			pageTable[k].SecondPageTable[i].auxAddr =k * PAGE_SUM * PAGE_SIZE +  i * PAGE_SIZE;
			int processNo = random() % number_process;
			pageTable[k].SecondPageTable[i].processNum = processNo;
			process_page[processNo][page_number_process[processNo]].FirstLevelNumber = k;
			process_page[processNo][page_number_process[processNo]++].SecondLevelNumber = i;
		}
	}
	int precord;
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* ���ѡ��һЩ��������ҳ��װ�� */
		if (random() % 2 == 0)
		{
			precord = random() % number_process;
			do_page_in(&pageTable[precord].SecondPageTable[j%64], j);
			pageTable[precord].SecondPageTable[j%64].blockNum = j;
			pageTable[precord].SecondPageTable[j%64].filled = TRUE;
			blockStatus[j%64] = TRUE;
		}
		else
			blockStatus[j] = FALSE;
	}
}


/* ��Ӧ���� */
void do_response()
{   int i,j=0;
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, offAddr;
	unsigned int actAddr;

	/* ����ַ�Ƿ�Խ�� */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* ����ҳ�ź�ҳ��ƫ��ֵ */
	pageNum = ptr_memAccReq->virAddr / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("һ��ҳ���Ϊ ��%lu\t����ҳ��Ϊ��%u\tҳ��ƫ��Ϊ��%u\n",ptr_memAccReq->virFirstLevelPage, pageNum, offAddr);

    for(i=0;i<page_number_process[ptr_memAccReq->processNum];i++)
        {
            if(process_page[ptr_memAccReq->processNum][i].FirstLevelNumber==ptr_memAccReq->virFirstLevelPage 
				&& process_page[ptr_memAccReq->processNum][i].SecondLevelNumber==pageNum)
            {
                j++;
                break;
            }
        }
    if(j==0)
    {
        do_error(ERROR_PROCESS_NUMBER);
        return;
    }


	/* ��ȡ��Ӧҳ���� */
	ptr_pageTabIt = &pageTable[ptr_memAccReq->virFirstLevelPage].SecondPageTable[pageNum];

	/* ��������λ�����Ƿ����ȱҳ�ж� */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}

	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("ʵ��ַΪ��%u\n", actAddr);

	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //������
		{
			ptr_pageTabIt->count=++LRU_NUM;
			if (!(ptr_pageTabIt->proType & READABLE)) //ҳ�治�ɶ�
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* ��ȡʵ���е����� */
			printf("�������ɹ���ֵΪ%c\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //д����
		{
			ptr_pageTabIt->count=++LRU_NUM;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //ҳ�治��д
			{
				do_error(ERROR_WRITE_DENY);
				return;
			}
			/* ��ʵ����д����������� */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;
			printf("д�����ɹ�\n");
			break;
		}
		case REQUEST_EXECUTE: //ִ������
		{
			ptr_pageTabIt->count=++LRU_NUM;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //ҳ�治��ִ��
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}
			printf("ִ�гɹ�\n");
			break;
		}
		default: //�Ƿ���������
		{
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* ����ȱҳ�ж� */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("����ȱҳ�жϣ���ʼ���е�ҳ...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* ���������ݣ�д�뵽ʵ�� */
			do_page_in(ptr_pageTabIt, i);

			/* ����ҳ������ */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = ++LRU_NUM;

			blockStatus[i] = TRUE;
			return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
	do_LRU(ptr_pageTabIt);
}

/* ����LRU�㷨����ҳ���滻 */
void do_LRU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min,FirstLevelPage, SecondLevelPage,j;
	printf("û�п�������飬��ʼ����LRUҳ���滻...\n");
	for (i = 0, min = 0xFFFFFFFF, FirstLevelPage = 0, SecondLevelPage = 0; i < number_process; i++)
	{
		for(j=0;j<PAGE_SUM;j++)
		{
			if (pageTable[i].SecondPageTable[j].count < min && pageTable[i].SecondPageTable[j].filled==TRUE)
			{
				min = pageTable[i].SecondPageTable[j].count;
				FirstLevelPage = i;
				SecondLevelPage = j;
			}
		}
	}
	printf("ѡ���һ��%uҳ�ڶ�ҳ%uҳ�����滻\n", FirstLevelPage,SecondLevelPage);
	if (pageTable[FirstLevelPage].SecondPageTable[SecondLevelPage].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageTable[FirstLevelPage].SecondPageTable[SecondLevelPage]);
	}
	pageTable[FirstLevelPage].SecondPageTable[SecondLevelPage].filled = FALSE;
	pageTable[FirstLevelPage].SecondPageTable[SecondLevelPage].count = ++LRU_NUM;


	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[FirstLevelPage].SecondPageTable[SecondLevelPage].blockNum);

	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[FirstLevelPage].SecondPageTable[SecondLevelPage].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = LRU_NUM;
	printf("ҳ���滻�ɹ�\n");
}

/* ����������д��ʵ�� */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	int i;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE,
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	block_auxAddr[blockNum] = ptr_pageTabIt->auxAddr;
	printf("��ҳ�ɹ��������ַ%lu-->>�����%u ��������Ϊ\n", ptr_pageTabIt->auxAddr, blockNum);
	for(i=0;i<4;i++)
	{
        printf("%c",*(actMem + blockNum * PAGE_SIZE+i));
	}
	printf("\n");
}

/* �����滻ҳ�������д�ظ��� */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE,
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	block_auxAddr[ptr_pageTabIt->blockNum] = -1;
	printf("д�سɹ��������%d-->>�����ַ%lu\n", ptr_pageTabIt->blockNum, ptr_pageTabIt->auxAddr);
}

/* ������ */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ��ɶ�\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���д\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���ִ��\n");
			break;
		}
		case ERROR_INVALID_REQUEST:
		{
			printf("�ô�ʧ�ܣ��Ƿ��ô�����\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("�ô�ʧ�ܣ���ַԽ��\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("ϵͳ���󣺴��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("ϵͳ���󣺹ر��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("ϵͳ�����ļ�ָ�붨λʧ��\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("ϵͳ���󣺶�ȡ�ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("ϵͳ����д���ļ�ʧ��\n");
			break;
		}
		case ERROR_PROCESS_NUMBER:
		{
			printf("�ô�ʧ�ܣ���ҳ�治�ܱ�ָ�����̷���\n");
			break;
		}
		default:
		{
			printf("δ֪����û������������\n");
		}
	}
}


/* ��ӡҳ�� */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("һ��ҳ�Ŷ���ҳ�ſ��\tװ��\t�޸�\t����\t����\t����\t���̺�\n");
	for(j = 0; j < number_process; j++){
		for (i = 0; i < PAGE_SUM; i++)
		{
			printf("%u \t%u \t%u\t%u\t%u\t%s\t%lu\t%lu\t%u\n", j, i, pageTable[j].SecondPageTable[i].blockNum, pageTable[j].SecondPageTable[i].filled,
				pageTable[j].SecondPageTable[i].edited, get_proType_str(str, pageTable[j].SecondPageTable[i].proType),
				pageTable[j].SecondPageTable[i].count, pageTable[j].SecondPageTable[i].auxAddr,pageTable[j].SecondPageTable[i].processNum);
		}
	}
}

/* ��ȡҳ�汣�������ַ��� */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}

int main(int argc, char* argv[])
{
	char c;
	unsigned int i;
	int fifo, last_mtime, mtime, size, out;
	char string[10000]="\0";
	unsigned int proccessNum, a;
	unsigned long virAddr, virFirstLevelPage;
    char value;
	FILE *fd;
	struct stat statbuf;
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}

	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* ��ѭ����ģ��ô������봦����� */
	while (TRUE)
	{
		char k;
        unsigned int i,j;
        if((fd = fopen("/tmp/fifo","w")) == NULL)
       		printf("I'm so sorry,Open file error.\n");
	fclose(fd);
	fifo=open("/tmp/fifo",O_RDONLY);
	fstat(fifo,&statbuf);
	last_mtime=statbuf.st_mtime;
	printf("�ȴ�������\n");
	while(TRUE){
		int num;
		FILE *ptr_temp;
		char temp[4];
		int ite;
		int readNum;
		fifo=open("/tmp/fifo",O_RDONLY);
		fstat(fifo,&statbuf);
		mtime=statbuf.st_mtime;
		size = statbuf.st_size;
		if(mtime!=last_mtime && size!=0){
			for(ite=0;ite<100;ite++)
			{
				string[ite]='\0';
			}
			out=read(fifo,string,100);
			//printf("%s",string);
			sscanf(string,"%d\t%lu\t%c\t%d\t%lu",&a, &virAddr, &value, &proccessNum, &virFirstLevelPage);
			if(a==0)
				ptr_memAccReq->reqType = REQUEST_READ;
			else if(a==1)
				ptr_memAccReq->reqType = REQUEST_WRITE;
			else
				ptr_memAccReq->reqType = REQUEST_EXECUTE;
			ptr_memAccReq->virAddr = virAddr;
			ptr_memAccReq->processNum = proccessNum;
			ptr_memAccReq->value = value;
			ptr_memAccReq->virFirstLevelPage = virFirstLevelPage;
			/*printf("%d\n",a);
			printf("%lu\n",ptr_memAccReq->virAddr);
			printf("%c\n",value);
			printf("%d\n",ptr_memAccReq->processNum);
			printf("%lu\n",ptr_memAccReq->virFirstLevelPage);*/
			do_response();
			printf("��A��ӡҳ��,��B��ӡʵ��,��C��ӡ����,��D��ӡ�����ܹ����ʵ�ҳ���,��Y���˳�����,������������ӡ\n");
			if ((c = getchar()) == 'a' || c == 'A')
				do_print_info();

			else if(c=='b'||c=='B')
			{
				int num1;
				int num2;
				printf("������\t�����ַ\tһ��ҳ��\t����ҳ��\t����\t\n");
				for(i=0;i<BLOCK_SUM;i++)
				{
					if(block_auxAddr[i]==-1)
						printf("%d\t\t----\t\t----\t\t----\t\t----\n",i);
					else
					{
						num1 = block_auxAddr[i]/(64*4);
						num2 = ((block_auxAddr[i]-num1*64*4)/4);
						printf("%d\t\t%d\t\t%d\t\t%d\t\t",i,block_auxAddr[i],num1,num2);
						for(j=0;j<4;j++)
						{
							printf("%c",actMem[j+i*4]);
						}
						printf("\n");
					}
				}
			}

			else if(c=='c'||c=='C')
			{
				printf("��������Ҫ���ʵĸ����ַ\n");
				scanf("%d",&num);
				if (fseek(ptr_auxMem, num, SEEK_SET) < 0)
				{
				#ifdef DEBUG
					printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
				#endif
					do_error(ERROR_FILE_SEEK_FAILED);
					exit(1);
				}
				if ((readNum = fread(temp,sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
				{
				#ifdef DEBUG
					printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
					printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
					printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
				#endif
					do_error(ERROR_FILE_READ_FAILED);
					exit(1);
				}

				for(j=0;j<4;j++)
				{
					printf("%c",temp[j]);
				}
				printf("\n");
			}
			else if(c=='D'||c=='d')
			{
			    for(j=0;j<number_process;j++)
			    {
			        printf("%d�Ž����ܹ����ʣ�\n",j);
			        for(i=0;i<page_number_process[j];i++)
			        {
			            printf("һ��ҳ�� : %d ����ҳ�� : %d\n",process_page[j][i].FirstLevelNumber , process_page[j][i].SecondLevelNumber);
			        }
			        printf("\n");
			    }
			}
			else if(c=='Y'||c=='y')
			{
				return 0;
			}
			else
			{
				;
			}
			printf("�ȴ�������\n");
			while (c != '\n')
				c = getchar();
		}
		last_mtime=statbuf.st_mtime;
		sleep(3);
	}
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
