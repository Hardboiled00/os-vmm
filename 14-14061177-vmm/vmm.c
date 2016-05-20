#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "vmm.h"
/* ҳĿ¼ */
PageIndexItem pageIndex[Process_SUM][INDEX_SUM];
/* ҳ�� */
//PageTableItem pageTable[PAGE_SUM];
/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq,head=NULL;



/* ��ʼ������ */
void do_init()
{
	int i, j,k,r,q,processCount,c;
	srandom(time(NULL));
	for(processCount=0;processCount<Process_SUM;processCount++)
	for (k = 0; k < INDEX_SUM; k++)
	for(i=0;i<INDEX_PAGE;i++)
	{
		pageIndex[processCount][k].processNum=processCount;
		pageIndex[processCount][k].indexNum=k;
		pageIndex[processCount][k].index[i].pageNum = i;
		pageIndex[processCount][k].index[i].filled = FALSE;
		pageIndex[processCount][k].index[i].edited = FALSE;
		pageIndex[processCount][k].index[i].visited = 0;
		for(c = 0;c < 8;c++){
            pageIndex[processCount][k].index[i].count[c] = '0';
		}
		/* ʹ����������ø�ҳ�ı������� */
		switch (random() % 7)
		{
			case 0:
			{
				pageIndex[processCount][k].index[i].proType = READABLE;
				//pageTable[i].proType = READABLE;
				break;
			}
			case 1:
			{
				pageIndex[processCount][k].index[i].proType = WRITABLE;
				break;
			}
			case 2:
			{
				pageIndex[processCount][k].index[i].proType =  EXECUTABLE;
				break;
			}
			case 3:
			{
				pageIndex[processCount][k].index[i].proType= READABLE | WRITABLE;
				break;
			}
			case 4:
			{
				pageIndex[processCount][k].index[i].proType= READABLE | EXECUTABLE;
				break;
			}
			case 5:
			{
				pageIndex[processCount][k].index[i].proType= WRITABLE | EXECUTABLE;
				break;
			}
			case 6:
			{
				pageIndex[processCount][k].index[i].proType= READABLE | WRITABLE | EXECUTABLE;
				break;
			}
			default:
				break;
		}
		/* ���ø�ҳ��Ӧ�ĸ����ַ */
		pageIndex[processCount][k].index[i].auxAddr = processCount*256+(k*INDEX_PAGE+i)*4;
		//pageTable[i].auxAddr = i * PAGE_SIZE ;
	}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* ���ѡ��һЩ��������ҳ��װ�� */
		if (random() % 2 == 0)
		{
			q = j/INDEX_PAGE;
			r = j%INDEX_PAGE;
			do_page_in(&pageIndex[0][q].index[r], j);
			pageIndex[0][q].index[r].blockNum = j;
			pageIndex[0][q].index[r].filled = TRUE;
			blockStatus[j] = TRUE;
		}
		else
			blockStatus[j] = FALSE;
	}
}


/* ��Ӧ���� */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, offAddr,indexNum,ProcessNum;
	unsigned int actAddr;
	Ptr_MemoryAccessRequest aptr_memAccReq;
	int processCount, k, i, c;

	if(head==NULL){
	printf("hi\n");
		return;}
	aptr_memAccReq = head;
	head = head->next;
	ProcessNum =  aptr_memAccReq->ProcessNum;
	/* ����ַ�Ƿ�Խ�� */
	if (aptr_memAccReq->virAddr < aptr_memAccReq->ProcessNum*256 || aptr_memAccReq->virAddr >= (aptr_memAccReq->ProcessNum+1)*256)
	{
		printf("viraddr=%d\t,processnum=%d\n",aptr_memAccReq->virAddr,ProcessNum);
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* ����ҳ�ź�ҳ��ƫ��ֵ */

	pageNum = (aptr_memAccReq->virAddr-ProcessNum*256) / PAGE_SIZE % INDEX_PAGE ;
	indexNum = (aptr_memAccReq->virAddr-ProcessNum*256) / PAGE_SIZE / INDEX_PAGE;
	offAddr = (aptr_memAccReq->virAddr-ProcessNum*256) % PAGE_SIZE;

	printf("���̺�Ϊ��%u\tҳĿ¼Ϊ��%u\tҳ��Ϊ��%u\tҳ��ƫ��Ϊ��%u\n",ProcessNum,indexNum, pageNum, offAddr);

	/* ��ȡ��Ӧҳ���� */
	ptr_pageTabIt = &pageIndex[ProcessNum][indexNum].index[pageNum];
	/* ��������λ�����Ƿ����ȱҳ�ж� */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}

	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("ʵ��ַΪ��%u\n", actAddr);

    for(processCount=0;processCount<Process_SUM;processCount++)
        for (k = 0; k < INDEX_SUM; k++)
            for(i=0;i<INDEX_PAGE;i++)
            {
                pageIndex[processCount][k].index[i].visited = 0;
            }

	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (aptr_memAccReq->reqType)
	{
		case REQUEST_READ: //������
		{
			ptr_pageTabIt->visited = 1;
			for(processCount=0;processCount<Process_SUM;processCount++)
                for (k = 0; k < INDEX_SUM; k++)
                    for(i=0;i<INDEX_PAGE;i++)
                    {
                        for(c = 7;c > 0;c--){
                            pageIndex[processCount][k].index[i].count[c] = pageIndex[processCount][k].index[i].count[c - 1];
                        }
                        pageIndex[processCount][k].index[i].count[0] = pageIndex[processCount][k].index[i].visited + '0';
                    }
			if (!(ptr_pageTabIt->proType & READABLE)) //ҳ�治�ɶ�
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* ��ȡʵ���е����� */
			printf("�������ɹ���ֵΪ%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //д����
		{
			ptr_pageTabIt->visited = 1;
			for(processCount=0;processCount<Process_SUM;processCount++)
                for (k = 0; k < INDEX_SUM; k++)
                    for(i=0;i<INDEX_PAGE;i++)
                    {
                        for(c = 7;c > 0;c--){
                            pageIndex[processCount][k].index[i].count[c] = pageIndex[processCount][k].index[i].count[c - 1];
                        }
                        pageIndex[processCount][k].index[i].count[0] = pageIndex[processCount][k].index[i].visited + '0';
                    }
			if (!(ptr_pageTabIt->proType & WRITABLE)) //ҳ�治��д
			{
				do_error(ERROR_WRITE_DENY);
				return;
			}
			/* ��ʵ����д����������� */
			actMem[actAddr] = aptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;
			printf("д�����ɹ�\n");
			break;
		}
		case REQUEST_EXECUTE: //ִ������
		{
			ptr_pageTabIt->visited = 1;
			for(processCount=0;processCount<Process_SUM;processCount++)
                for (k = 0; k < INDEX_SUM; k++)
                    for(i=0;i<INDEX_PAGE;i++)
                    {
                        for(c = 7;c > 0;c--){
                            pageIndex[processCount][k].index[i].count[c] = pageIndex[processCount][k].index[i].count[c - 1];
                        }
                        pageIndex[processCount][k].index[i].count[0] = pageIndex[processCount][k].index[i].visited + '0';
                    }
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
	unsigned int i, c;
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
			ptr_pageTabIt->visited = 0;
			for(c = 0;c < 8;c++){
				ptr_pageTabIt->count[c] = '0';
			}

			blockStatus[i] = TRUE;
			return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
	do_PA(ptr_pageTabIt);
}
/* ����ҳ���ϻ��㷨����ҳ���滻 */
void do_PA(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i,k,s, min, page,index,processNum, c, temp_count;
	printf("û�п�������飬��ʼ����ҳ���ϻ��㷨ҳ���滻...\n");
	for(s=0,min=0xFFFFFFFF,page=0,index=0,processNum=0;s<Process_SUM;s++)
        for(k=0;k<INDEX_SUM;k++)
            for (i = 0; i < INDEX_PAGE; i++)
            {
                for(c = 0, temp_count = 0;c < 8;c++){
                    temp_count = temp_count * 10 + pageIndex[s][k].index[i].count[c] - '0';
                }
                if (temp_count < min && pageIndex[s][k].index[i].filled==TRUE)
                {
                    min = temp_count;
                    processNum=s;
                    page = i;
                    index=k;
                }
            }
    printf("ѡ���%u��Ŀ¼��%uҳ�����滻\n",index,page);
	if (pageIndex[processNum][index].index[page].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageIndex[processNum][index].index[page]);
	}
	pageIndex[processNum][index].index[page].filled = FALSE;
	pageIndex[processNum][index].index[page].visited = 0;
	for(c = 0;c < 8;c++){
		pageIndex[processNum][index].index[page].count[c] = '0';
	}

	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt,pageIndex[processNum][index].index[page].blockNum);

	/* ����ҳ������ */
	ptr_pageTabIt->blockNum =pageIndex[processNum][index].index[page].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->visited = 0;
	for(c = 0;c < 8;c++){
		ptr_pageTabIt->count[c] = '0';
	}
	printf("ҳ���滻�ɹ�\n");
}

/* ����������д��ʵ�� */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
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
	printf("��ҳ�ɹ��������ַ%u-->>�����%u\n", ptr_pageTabIt->auxAddr, blockNum);
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
	printf("д�سɹ��������%u-->>�����ַ%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
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
		default:
		{
			printf("δ֪����û������������\n");
		}
	}
}

/* �����ô����� */
void do_request(cmd acmd)
{
	Ptr_MemoryAccessRequest p;
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	ptr_memAccReq->virAddr = acmd.virAddr;
	ptr_memAccReq->ProcessNum = acmd.ProcessNum;
	printf("dorequest addr:=%ld\n",acmd.virAddr);
	ptr_memAccReq->reqType = acmd.reqType;
	if(acmd.reqType==REQUEST_WRITE)
		ptr_memAccReq->value = acmd.value;
	ptr_memAccReq->next = NULL;
	if(head==NULL)
		head = ptr_memAccReq;
	else{
		for(p=head;p->next!=NULL;p=p->next)
			;
		p->next = ptr_memAccReq;
	}
}

/* ��ӡҳ�� */
void do_print_info()
{
	unsigned int i, j, k,s,p,c;
	char str[4];
	printf("���̺�\tĿ¼��\tҳ��\t���\tװ��\t�޸�\t����\t����\t\t����\n");
	for(p=0;p<Process_SUM;p++)
	for(s=0;s<INDEX_SUM;s++)
	for (i = 0; i <INDEX_PAGE; i++)
	{
		printf("%u\t%u\t%u\t%u\t%u\t%u\t%s\t", p,s,i, pageIndex[p][s].index[i].blockNum, pageIndex[p][s].index[i].filled,
			pageIndex[p][s].index[i].edited, get_proType_str(str, pageIndex[p][s].index[i].proType));
        for(c = 0;c < 8;c++){
			printf("%c",pageIndex[p][s].index[i].count[c]);
		}
		printf("\t%u\n", pageIndex[p][s].index[i].auxAddr);
	}
}
/*��ӡʵ��*/
void do_print_act()
{
	int i;
	printf("print actual memeory\n");
	for(i = 0;i < ACTUAL_MEMORY_SIZE;i++){
		printf("%d\t%c\n",i,actMem[i]);
	}
}
/*��ӡ����*/
void do_print_vir()
{
	int i;
	char temp_byte;
	FILE* p = fopen("vmm_auxMem","r");
	printf("print virtual memory\n");
	for(i = 0;i < VIRTUAL_MEMORY_SIZE;i++){
        printf("%c",fgetc(p));
	}
	/*while((temp_byte = fgetc(p)) != EOF){
		printf("%c",temp_byte);
	}*/

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
	int i;
	int fifo2;
	int count;
	cmd req;
	struct stat statbuf;
	req.RT=NOR;
	if(stat("/tmp/doreq",&statbuf)==0){
		/* ���FIFO�ļ�����,ɾ�� */
		if(remove("/tmp/doreq")<0)
			printf("remove failed\n");
	}

	if(mkfifo("/tmp/doreq",0666)<0)
		printf("mkfifo failed\n");
	/* �ڷ�����ģʽ�´�FIFO */
	if((fifo2=open("/tmp/doreq",O_RDONLY|O_NONBLOCK))<0)
		printf("open fifo failed\n");

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
		if((count=read(fifo2,&req,CMDLEN))<0)
			printf("read fifo failed\n");
		switch(req.RT){
			case REQUEST: printf("addr=%ld\n",req.virAddr);
			do_request(req);req.RT=NOR;
			break;
			//case RESPONSE:printf("aaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
			//do_response();req.RT=NOR;
			//break;
			default:
			break;
		}
		do_response();
		printf("��Y��ӡҳ��������������ӡ...\n");
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
		while (c != '\n')
			c = getchar();
        printf("��A��ӡʵ�棬������������ӡ...\n");
		if ((c = getchar()) == 'a' || c == 'A')
			do_print_act();
		while (c != '\n')
			c = getchar();
        printf("��V��ӡ���棬������������ӡ...\n");
		if ((c = getchar()) == 'v' || c == 'V')
			do_print_vir();
		while (c != '\n')
			c = getchar();
		printf("��X�˳����򣬰�����������...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	close(fifo2);
	return (0);
}
