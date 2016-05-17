#include <stdio.h>
#include <stdlib.h>
#include<sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/stat.h>
#include <math.h>
#include <time.h>
#include "vmm.h"



/* ��ʼ������ */
void do_init()
{
	int i,j,n;
	srand(time(NULL));
	time_n=0;
    exec_times=0;
	for(n=0;n<OUTER_PAGE_TOTAL;n++)
	{
		outerpagetable[n].page_num=n;
		outerpagetable[n].index_num=n*PAGE_SIZE;
		
		for(i=n*PAGE_SIZE;i<(n+1)*PAGE_SIZE&&i<PAGE_TOTAL;i++)
		{
			pagetable[i].page_num=i;
			pagetable[i].filled=FALSE;
			pagetable[i].changed=FALSE;
			pagetable[i].count=0;
			/* ʹ����������ø�ҳ�ı������� */
			switch (rand()%7)
			{
				case 0:
					pagetable[i].pro_type=READABLE;
					break;
				case 1:
					pagetable[i].pro_type=WRITABLE;
					break;
				case 2:
					pagetable[i].pro_type=EXECUTABLE;
					break;
				case 3:
					pagetable[i].pro_type=READABLE|WRITABLE;
					break;
				case 4:
					pagetable[i].pro_type=READABLE|EXECUTABLE;
					break;
				case 5:
					pagetable[i].pro_type=WRITABLE|EXECUTABLE;
					break;
				case 6:
					pagetable[i].pro_type=READABLE|WRITABLE|EXECUTABLE;
					break;
				default:
					break;
			}
			/* ���ø�ҳ��Ӧ�ĸ����ַ */
			pagetable[i].virtual_address=i*PAGE_SIZE*2;
		}
	}
	for (j=0;j<BLOCK_TOTAL;j++)
	{
		/* ���ѡ��һЩ��������ҳ��װ�� */
		if(rand()%2==0)
		{
			do_page_in(&pagetable[j],j);
			pagetable[j].block_num=j;
			pagetable[j].filled=TRUE;
			//pagetable[j].no_use++;
			block_status[j]=TRUE;
			Time[time_n++]=pagetable[j].page_num;
		}
		else
			block_status[j]=FALSE;
	}

	//�涨��ҵ���Լ���Ӧ��ҳ���
	pcb[0].pid=1;
	pcb[0].begin=0;
	pcb[0].end=7;
	pcb[1].pid=2;
	pcb[1].begin=8;
	pcb[1].end=15;
}


/* ��Ӧ���� */
void do_response()
{
	PageTablePtr ptable;
	unsigned int outer_page_num,in_page_offset,offset,in_page_num,actual_address,i;

	if(mem_request->virtual_address<0||mem_request->virtual_address>VIRTUAL_MEMORY_SIZE)
	{
		handle_error(OVER_BOUNDARY);
		return;
	}
	
	outer_page_num=(mem_request->virtual_address/PAGE_SIZE)/PAGE_SIZE;		//һ��ҳ���
	in_page_offset=(mem_request->virtual_address/PAGE_SIZE)%PAGE_SIZE;		//����ƫ��
	offset=mem_request->virtual_address%PAGE_SIZE;							//ҳ��ƫ��
    
	for(i=0;i<PID_NUM;++i)
	{
		if(outer_page_num>=pcb[i].begin && outer_page_num<=pcb[i].end)
		{
			work_id=pcb[i].pid;												//�õ������
		}
	}
	in_page_num=outerpagetable[outer_page_num].index_num+in_page_offset;	//��õĶ���ҳ���
	printf("The program id: %u\tThe page number:%u\tThe offset:%u\n",work_id,in_page_num,offset);

	ptable=&pagetable[in_page_num];

	if(!ptable->filled)
	{
		do_page_fault(ptable);
	}

	ptable->no_use=exec_times++;
	actual_address=ptable->block_num*PAGE_SIZE+offset;
	printf("The actual address is: %u\n",actual_address);
	
	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (mem_request->request_type)
	{
		case READ:	/*������*/
		{
			ptable->count++;
			if(!(ptable->pro_type&READABLE))	/*ҳ�治�ɶ�*/
			{
				handle_error(READ_DENY);
				return;
			}
			/* ��ȡʵ���е����� */
			printf("Success to read: The value is %02X\n",actual_memory[actual_address]);
			break;
		}
		case WRITE:
		{
			ptable->count++;
			if(!(ptable->pro_type&WRITABLE))	/*ҳ�治��д*/
			{
				handle_error(WRITE_DENY);	
				return;
			}
			/* ��ʵ����д����������� */
			actual_memory[actual_address]=mem_request->value;
			ptable->changed=TRUE;			
			printf("Success to write!\n");
			break;
		}
		case EXECUTE:	/*ִ������*/
		{
			ptable->count++;
			if(!(ptable->pro_type&EXECUTABLE))	/*ҳ�治��ִ��*/
			{
				handle_error(EXECUTE_DENY);
				return;
			}			
			printf("Success to execute!\n");
			break;
		}
		default:	/*�Ƿ���������*/
		{	
			handle_error(INVALID_REQUEST);
			return;
		}
	}
}

/* ����ȱҳ�ж� */
void do_page_fault(PageTablePtr ptable)
{
    unsigned int i;
	char c;
    for(i=0;i<BLOCK_TOTAL;i++)
    {
        if(!block_status[i])
        {
			do_page_in(ptable, i);
			
			/* ����ҳ������ */
			ptable->block_num = i;
			ptable->filled = TRUE;
			ptable->changed = FALSE;
			ptable->count = 0;
			
			block_status[i] = TRUE;
			return;
        }
    }
	printf("Please choose a method to do the page out algorithm,and press '1' for FIFO, '2' for LFU, '3' for LRU...\n");
	while(c=getchar())
	{
		if(c=='1')
		{
			do_FIFO(ptable);
			break;
		}
		else if(c=='2')
		{
			do_LFU(ptable);
			break;
		}
		else if(c=='3')
		{
			do_LRU(ptable);
			break;
		}
	}
}

/* ����LFU�㷨����ҳ���滻 */
void do_LFU(PageTablePtr ptable)
{
    unsigned int i,min_use,page;
    printf("There is no idle block.Do LFU:\n ");
    min_use=0xFFFFFFFF;//����СƵ�ʽ��г�ʼ��
    page=0;
    for(i=0;i<PAGE_TOTAL;i++)
    {
        if(pagetable[i].count<min_use&&pagetable[i].filled==TRUE)
        {
            min_use=pagetable[i].count;
            page=pagetable[i].page_num;
        }
    }
    printf("Replace the %uth page./n",page);
    if(pagetable[page].changed)
    {
        printf("The page to be replaced has been changed:Write back.\n");
        do_page_out(&pagetable[page]);
    }
    pagetable[page].changed=FALSE;
    pagetable[page].count=0;
    pagetable[page].filled=FALSE;


    do_page_in(ptable,pagetable[page].block_num);


    ptable->block_num=pagetable[page].block_num;
    ptable->changed=FALSE;
    ptable->count=0;
	//ptable->no_use=0;
    ptable->filled=TRUE;
	printf("LFU success\n");
}
/* ����FIFO�㷨����ҳ���滻 */
void do_FIFO(PageTablePtr ptable)
{
    unsigned int firstcome;
    firstcome=Time[0];									//Time[PAGE_TOTAL]����������ڵ��������ҳ��洢״����ÿһ����ҳ�ţ���һ�����Ƚ�����ҳ��
    printf("There is no idle block. Do FIFO:\n ");
    printf("Replace the %uth page./n",firstcome);
    if(pagetable[firstcome].changed)
    {
          printf("The page to be replaced has been changed: Write back.\n");
          do_page_out(&pagetable[firstcome]);
    }
    pagetable[firstcome].changed=FALSE;
    pagetable[firstcome].count=0;
	pagetable[firstcome].no_use=0;
    pagetable[firstcome].filled=FALSE;

    do_page_in(ptable,pagetable[firstcome].block_num);


    ptable->block_num=pagetable[firstcome].block_num;
    ptable->changed=FALSE;
    ptable->count=0;
	//ptable->no_use=0;
    ptable->filled=TRUE;

	time_change(ptable->page_num);
	printf("FIFO success\n");
}
void do_LRU(PageTablePtr ptable)
{
    unsigned int i,min_use,page;
    printf("There is no idle block.Do LRU..\n ");
    min_use=0xFFFFFFFF;//����СƵ�ʽ��г�ʼ��
    page=0;
    for(i=0;i<PAGE_TOTAL;i++)
    {
        if(min_use>pagetable[i].no_use&&pagetable[i].filled==TRUE)
        {
            min_use=pagetable[i].no_use;
            page=pagetable[i].page_num;
        }
    }
    printf("Replace the %uth page./n",page);
    if(pagetable[page].changed)
    {
        printf("The page to be replaced has been changed:Write back.\n");
        do_page_out(&pagetable[page]);
    }
    pagetable[page].changed=FALSE;
    pagetable[page].count=0;
    pagetable[page].no_use=0;
    pagetable[page].filled=FALSE;


    do_page_in(ptable,pagetable[page].block_num);


    ptable->block_num=pagetable[page].block_num;
    ptable->changed=FALSE;
    ptable->count=0;
    ptable->no_use=0;
    ptable->filled=TRUE;

	time_change(ptable->page_num);
	printf("LRU success\n");
}

/* ����������д��ʵ�� */
void do_page_in(PageTablePtr ptable,unsigned int block_num)
{
	unsigned int read_num;
	if(fseek(auxmem_ptr,ptable->virtual_address,SEEK_SET)<0)
	{
		handle_error(FILE_SEEK_FAILED);
		exit(1);
	}
	if((read_num=fread(&actual_memory[block_num*PAGE_SIZE],sizeof(unsigned char),PAGE_SIZE,auxmem_ptr))<PAGE_SIZE)
	{
		handle_error(FILE_READ_FAILED);
		exit(1);
	}
	printf("Read page success: auxiliary memory address %u -->> actual block %u\n",ptable->virtual_address,block_num);
}
void time_change(unsigned int num)
{
	int i;
	for(i=0;i<time_n-1;++i)
		Time[i]=Time[i+1];
	Time[time_n]=num;
}
/* �����滻ҳ�������д�ظ��� */
void do_page_out(PageTablePtr ptable)
{
	unsigned int write_num;
	if(fseek(auxmem_ptr,ptable->virtual_address,SEEK_SET)<0)
	{
		handle_error(FILE_SEEK_FAILED);
		exit(1);
	}
	if((write_num=fwrite(&actual_memory[ptable->block_num*PAGE_SIZE],sizeof(unsigned char),PAGE_SIZE,auxmem_ptr))<PAGE_SIZE)
	{
		handle_error(FILE_WRITE_FAILED);
		exit(1);
	}
	printf("Write back success: actual block %u -->> auxiliary address %03X\n",ptable->virtual_address,ptable->block_num);
}

/* ������ */
void handle_error(ErrorType error_type)
{
	switch (error_type)
	{
		case READ_DENY:
			printf("Memory access failed : Contents of the address is unreadable\n\n");
			break;
		case WRITE_DENY:
			printf("Memory access failed : Contents of the address is denied to be writen\n");
			break;
		case EXECUTE_DENY:
			printf("Memory access failed : Contents of the address is denied to execute\n");
			break;
		case INVALID_REQUEST:
			printf("Memory access failed : Invalid memory access request\n");
			break;
		case OVER_BOUNDARY:
			printf("Memory access failed : The array is out of boundary\n");
			break;
		case FILE_OPEN_FAILED:
			printf("System error : Failed to open the file\n");
			break;
		case FILE_CLOSE_FAILED:
			printf("System error : Failed to close the file\n");
			break;
		case FILE_SEEK_FAILED:
			printf("System error : Failed to location the file seek\n");
			break;
		case FILE_READ_FAILED:
			printf("System error : Failed to read the file\n");
			break;
		case FILE_WRITE_FAILED:
			printf("System error : Failed to write in the file\n");
			break;
		default:
			printf("Exception occured : System unkown error\n");
			break;
	}
}
/* �����ô����� */
/*void do_request()
{
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	switch (random() % 3)
	{
		case 0: //������
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("��������\n��ַ��%u\t���ͣ���ȡ\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //д����
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			ptr_memAccReq->value = random() % 0xFFu;
			printf("��������\n��ַ��%u\t���ͣ�д��\tֵ��%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("��������\n��ַ��%u\t���ͣ�ִ��\n", ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}	
}
*/
/* ��ӡҳ�� */
void print_pageinfo()
{
	unsigned int i,j,m;
	unsigned char str[4];
	printf("oNO.\tiNO.\tbNO.\tFilled\tChanged\tPro\tCnt\tAux mem\n");
	for(i=0;i<OUTER_PAGE_TOTAL;++i)
	{
		for(j=0;j<PAGE_SIZE;++j)
		{
			m=outerpagetable[i].index_num+j;
			printf("%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i, pagetable[m].page_num,pagetable[m].block_num, pagetable[m].filled, 
				pagetable[m].changed, get_protype_str(str, pagetable[m].pro_type), 
				pagetable[m].count, pagetable[m].virtual_address);
		}
	}
}

/* ��ȡҳ�汣�������ַ��� */
char *get_protype_str(char *str,unsigned char type)
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
	int i,read_num=0;
	FILE *fp;

	if(!(auxmem_ptr=fopen("vmm_auxMem","r+")))
	{
		handle_error(FILE_OPEN_FAILED);
		exit(1);
	}
	
	do_init();
	print_pageinfo();
	mem_request=(MemoryAccessRequestPtr)malloc(sizeof(MemoryAccessRequest));
	umask(0);									//�ļ�������
	mkfifo(FIFO,S_IFIFO | 0666);				//��FIFO����ȡ����
	fp=fopen(FIFO,"r");				//����FIFO
	/* ��ѭ����ģ��ô������봦����� */
	while(TRUE)
	{
		//do_request();
		       						
		if((read_num=fread(mem_request,sizeof(MemoryAccessRequest),1,fp))==0)
		{
			printf("Please produce memory access request...");
			printf("Press 'X' to cancel, Press others to continue...\n");
			if((c=getchar())=='x'||c=='X')
				break;
			else
				continue;
		}

		do_response();
		printf("Press 'Y' to print the page-table...\n");
		if((c=getchar())=='y'||c=='Y')
			print_pageinfo();
		while(c!='\n')
			c=getchar();
		printf("Press 'X' to cancel, Press others to continue...\n");
		if((c=getchar())=='x'||c=='X')
			break;
		while(c!='\n')
			c=getchar();
		/*sleep(5000);*/
	}
	
	fclose(fp);
	if(fclose(auxmem_ptr)==EOF)
	{
		handle_error(FILE_CLOSE_FAILED);
		exit(1);
	}
    return 0;
}
