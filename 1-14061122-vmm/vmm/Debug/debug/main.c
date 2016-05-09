#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "vmm.h"


/* ҳ�� */
PageTableItem pageTable[PAGE_SUM];

/*******************һ��ҳ��********************/
FPageTableItem fpageTable[FPageNum];

/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq;

int Operate[64];

int N  =  0;

time_t rtime;
/* ��ʼ������ */
void do_init()
{
	int i, j,k,h,l;
	srand(time(NULL));
	for (i = 0; i < PAGE_SUM; i++)
	{
		pageTable[i].pageNum = i;
		pageTable[i].filled = FALSE;
		pageTable[i].edited = FALSE;
		pageTable[i].count = 0;
		/* ʹ����������ø�ҳ�Ŀɷ��ʽ��̺� */
		for(l=0;l<Process_SIZE;l++)
        {
            switch (rand() % 2)
            {
                case 0:
                    {
                        pageTable[i].processNum[l]=FALSE;
                        break;
                    }
                case 1:
                    {
                        pageTable[i].processNum[l]=TRUE;
                        break;
                    }
            }
        }
        BOOL flag=FALSE;
        for(l=0;l<Process_SIZE;l++)
        {
            if(pageTable[i].processNum[l]==TRUE)
                flag=TRUE;
        }
        if(flag==FALSE)
        {
            int m=rand() % Process_SIZE;
            pageTable[i].processNum[m]=TRUE;
        }
		/* ʹ����������ø�ҳ�ı������� */
		switch (rand() % 7)
		{
			case 0:
			{
				pageTable[i].proType = READABLE;
				break;
			}
			case 1:
			{
				pageTable[i].proType = WRITABLE;
				break;
			}
			case 2:
			{
				pageTable[i].proType = EXECUTABLE;
				break;
			}
			case 3:
			{
				pageTable[i].proType = READABLE | WRITABLE;
				break;
			}
			case 4:
			{
				pageTable[i].proType = READABLE | EXECUTABLE;
				break;
			}
			case 5:
			{
				pageTable[i].proType = WRITABLE | EXECUTABLE;
				break;
			}
			case 6:
			{
				pageTable[i].proType = READABLE | WRITABLE | EXECUTABLE;
				break;
			}
			default:
				break;
		}
		/* ���ø�ҳ��Ӧ�ĸ����ַ */
		pageTable[i].auxAddr = i * PAGE_SIZE;
	}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* ���ѡ��һЩ��������ҳ��װ�� */
		if (rand() % 2 == 0)
		{
			do_page_in(&pageTable[j], j);
			pageTable[j].blockNum = j;
			pageTable[j].filled = TRUE;
			time(&rtime);
			pageTable[j].time = rtime;
			blockStatus[j] = TRUE;
		}
		else
			blockStatus[j] = FALSE;
	}
	for (k=0; k < FPageNum; k++){
		for(h=0;h<(PAGE_SUM/FPageNum);h++){
			fpageTable[k].page[h]=&pageTable[(k*16)+h];
		}

	}
	for(h=0;h<64;h++){
		Operate[h]=-1;
	}
}

/******************************ҳ���ϻ�*****************************/
void do_old(Ptr_PageTableItem ptr_pageTabIt){

	int i = 0,min = 0,mark=0,page;

	printf("ȱҳ��ִ��ҳ���ϻ��㷨...\n");
	min  = pageTable[0].time;
	for(i = 1;i<64;i++){
		if(min>pageTable[i].time){
			min  = pageTable[i].time;
			mark = i;
		}
	}
	printf("һ��ҳ���%d����ҳ���%d\n",mark/16,mark%16);
	page = mark;
	if (pageTable[page].edited)
	{

		printf("��ҳ���޸ģ�д��\n");
		do_page_out(&pageTable[page]);
	}
	pageTable[page].filled = FALSE;
	pageTable[page].count = 0;


	/* ?��?����??����Y��?D�䨨?��?���̡�? */
	do_page_in(ptr_pageTabIt, pageTable[page].blockNum);

	/* ?��D?��3����?����Y */
	ptr_pageTabIt->blockNum = pageTable[page].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	time(&rtime);
	ptr_pageTabIt->time = rtime;
	ptr_pageTabIt->count = 0;
	printf("ҳ���滻�ɹ�\n");
}

/* ��Ӧ���� */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, offAddr;
	unsigned int actAddr;

	/*��������Ƿ�Խ��*/
	if((ptr_memAccReq->reqType>2)||(ptr_memAccReq->reqType<0)){
		return;

	}


	/* ����ַ�Ƿ�Խ�� */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* ����ҳ�ź�ҳ��ƫ��ֵ */
	pageNum = ptr_memAccReq->virAddr / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("ҳ��Ϊ��%u\tҳ��ƫ��Ϊ��%u\n", pageNum, offAddr);

	/* ��ȡ��Ӧҳ���� */
	ptr_pageTabIt = &pageTable[pageNum];

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
			ptr_pageTabIt->count++;
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
			ptr_pageTabIt->count++;
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
			ptr_pageTabIt->count++;
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
/***************************************�༶ҳ����*******************************/
void do_responsemult(){
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int fpageNum,npageNum,offAddr;
	unsigned int actAddr;
	unsigned int temp;
	/*��������Ƿ�Խ��*/
	if((ptr_memAccReq->reqType>2)||(ptr_memAccReq->reqType<0)){
		return;
	}
	/* ����ַ�Ƿ�Խ�� */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= PageTableItem_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* ����ҳ�ź�ҳ��ƫ��ֵ */
	fpageNum = ptr_memAccReq->virAddr / FPage_SIZE;
	temp  = ptr_memAccReq->virAddr - fpageNum*FPage_SIZE;
	npageNum = temp / NPage_SIZE;
	offAddr = temp % NPage_SIZE;
	if((fpageNum>3)||(npageNum>15)||(offAddr>3)){
		do_error(ERROR_INVALID_REQUEST);
		return;
	}

	printf("һ��ҳ���Ϊ��%u\t����ҳ���Ϊ�� %u\tҳ��ƫ��Ϊ��%u\n", fpageNum, npageNum,offAddr);

	/* ��ȡ��Ӧҳ���� */

	ptr_pageTabIt = fpageTable[fpageNum].page[npageNum];

    /* �жϽ����ܷ���� */
    if(ptr_pageTabIt->processNum[ptr_memAccReq->ProcessNum]==FALSE)
    {
        do_error(ERROR_PROCESS_DENY);
        return;
    }


	/* ��������λ�����Ƿ����ȱҳ�ж� */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	else {
		Operate[N] = fpageNum*16+npageNum;
		N=(N+1)%64;
	}


	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("ʵ��ַΪ��%u\n", actAddr);

	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //������
		{
			ptr_pageTabIt->count++;
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
			ptr_pageTabIt->count++;
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
			ptr_pageTabIt->count++;
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
			ptr_pageTabIt->count = 0;
			time(&rtime);
			ptr_pageTabIt->time = rtime;
			Operate[N] = i;
			N=(N+1)%64;
			blockStatus[i] = TRUE;
			return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
	do_old(ptr_pageTabIt);
}

/* ����LFU�㷨����ҳ���滻 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min, page;
	printf("û�п�������飬��ʼ����LFUҳ���滻...\n");
	for (i = 0, min = 0xFFFFFFFF, page = 0; i < PAGE_SUM; i++)
	{
		if (pageTable[i].count < min)
		{
			min = pageTable[i].count;
			page = i;
		}
	}
	printf("ѡ���%uҳ�����滻\n", page);
	if (pageTable[page].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageTable[page]);
	}
	pageTable[page].filled = FALSE;
	pageTable[page].count = 0;


	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[page].blockNum);

	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[page].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("ҳ���滻�ɹ�\n");
}


/************************************LRU�㷨************************************/
void do_LRU(Ptr_PageTableItem ptr_pageTabIt){
	int lru[64]={0};
	int i = 0;
	int flag = 0;
	int fpagenum,npagenum,page;
	printf("û�п�������飬��ʼ����LRUҳ���滻...\n");

	for(i=0;i<64;i++){
		if(Operate[i]!=-1){
			lru[Operate[i]] = 1;
		}
	}

	for(i=0;i<64;i++){
		if(lru[i]==0){
			fpagenum = i/16;
			npagenum = i%16;
			flag = 1;
			break;
		}
	}
	if(flag = 0){
		fpagenum = rand()%4;
		npagenum = rand()%16;

	}
	printf("ѡ���%d���%dҳ�����滻\n",fpagenum,npagenum);
	page = fpagenum*16+npagenum;
	Operate[N] = page;
	N = (N+1)%64;

	if (pageTable[page].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageTable[page]);
	}
	pageTable[page].filled = FALSE;
	pageTable[page].count = 0;


	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[page].blockNum);

	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[page].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
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
		case ERROR_PROCESS_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ��ǰ����û��Ȩ�޷���\n");
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
void do_request()
{
	/* ������������ַ */
	unsigned fpagenum,npagenum,offAddr;

	fpagenum = rand() % 4;
	npagenum = rand() % 16;
	offAddr = rand() % 4;

	ptr_memAccReq->virAddr = fpagenum*FPage_SIZE+npagenum*NPage_SIZE+offAddr;
	ptr_memAccReq->ProcessNum=rand()%Process_SIZE;
	/* ��������������� */
	switch (rand() % 3)
	{
		case 0: //������
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("��������\n��ַ��%u\t���ͣ���ȡ\t���̺ţ�%u\n", ptr_memAccReq->virAddr,ptr_memAccReq->ProcessNum);
			break;
		}
		case 1: //д����
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* ���������д���ֵ */
			ptr_memAccReq->value = rand() % 0xFFu;
			printf("��������\n��ַ��%u\t���ͣ�д��\tֵ��%02X\t���̺ţ�%u\n", ptr_memAccReq->virAddr, ptr_memAccReq->value,ptr_memAccReq->ProcessNum);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("��������\n��ַ��%u\t���ͣ�ִ��\t���̺ţ�%u\n", ptr_memAccReq->virAddr,ptr_memAccReq->ProcessNum);
			break;
		}
		default:
			break;
	}
}

/***************************************�ֶ���������*********************************************/
void in_request(){
	int i;
	/*������ַ*/
	printf("�����������ַ��\n");
	scanf("%d",&i);
	if((i>=VIRTUAL_MEMORY_SIZE)||(i<0)){
		printf("��ַ������Χ��\n");
		return;
	}
	ptr_memAccReq->virAddr=i;
	/*�������*/
	printf("������������̺ţ���0-%d��\n",Process_SIZE-1);
	scanf("%d",&i);
	if(i>=0&&i<Process_SIZE)
    {
        ptr_memAccReq->ProcessNum=i;
    }
    else
    {
        printf("���̺Ų��ڷ�Χ��\n");
			return;
    }
	/*��������*/
	printf("�������������ͣ���0Ϊ������1Ϊд����2Ϊִ������\n");
	scanf("%d",&i);
	if(i==0){
		ptr_memAccReq->reqType = REQUEST_READ;
		printf("��������\n��ַ��%u\t���̺ţ�%d\t���ͣ���ȡ\n", ptr_memAccReq->virAddr,ptr_memAccReq->ProcessNum);

	}
	else if(i==1){
		ptr_memAccReq->reqType = REQUEST_WRITE;
		/* ������д���ֵ */
		scanf("%d",&i);
		if((i<0)||(i>=0xFFu)){
			printf("д�����ݴ���\n");
			return;
		}
		ptr_memAccReq->value = rand() % 0xFFu;
		printf("��������\n��ַ��%u\t���̺ţ�%d\t���ͣ�д��\tֵ��%02X\n", ptr_memAccReq->virAddr,ptr_memAccReq->ProcessNum, ptr_memAccReq->value);
	}
	else if(i==2){
		ptr_memAccReq->reqType = REQUEST_EXECUTE;
		printf("��������\n��ַ��%u\t���̺ţ�%d\t���ͣ�ִ��\n", ptr_memAccReq->virAddr,ptr_memAccReq->ProcessNum);
	}
	else{
		printf("�������ʹ��󣡣���\n");
	}
}

/* ��ӡҳ�� */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	char str1[Process_SIZE+1];
	printf("һ����\t������\t���\tװ��\t�޸�\t����\t����\t���̺�\t����\n");
	for(i=0;i<FPageNum;i++){
		for(j=0;j<(PAGE_SUM/FPageNum);j++){
			printf("%u\t%u\t%u\t%u\t%u\t%s\t%u\t%s\t%u\n", i,j, pageTable[i*16+j].blockNum, pageTable[i*16+j].filled,
			pageTable[i*16+j].edited, get_proType_str(str, pageTable[i*16+j].proType),
			pageTable[i*16+j].count, get_processNum_str(str1,pageTable[i*16+j]),pageTable[i*16+j].auxAddr);
		}
	}
/*	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i, pageTable[i].blockNum, pageTable[i].filled,
			pageTable[i].edited, get_proType_str(str, pageTable[i].proType),
			pageTable[i].count, pageTable[i].auxAddr);
	}*/
}
/********************��ӡʵ��*****************/
void do_print_actMem(){
	int i;
	for(i=0;i<ACTUAL_MEMORY_SIZE;i++){
		printf("%d:   %c\n",i,actMem[i] );
	}
}
/*******************��ӡ����****************/
void do_print_auxMem(){
	char c;
	while((c=fgetc(ptr_auxMem))!=EOF) putchar(c);

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
/********************��ȡ���̺��ַ���*****************/
char *get_processNum_str(char *str,PageTableItem page)
{
	int i;
	for(i=0;i<Process_SIZE;i++)
    {
        if (page.processNum[i]==TRUE)
            str[i] = i+48;
        else
            str[i] = '-';
    }
	str[Process_SIZE] = '\0';
	return str;
}

int main(int argc, char* argv[])
{
	char* c;
	int i;
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}

	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* ��ѭ����ģ��ô������봦����� */
	char strm[2]="m\0";
		char strp[2]="p\0";
		char stra[2]="a\0";
		char strx[2]="x\0";
		char stre[2]="e\0";
	while (TRUE)
	{
		printf("��M�л����ֶ�����,��P��ӡҳ��,��A��ӡʵ��,��X��ӡ����,��E�˳�,���������Զ�����...\n");
		fgets(c,256,stdin);
		if(stricmp(c,strm)){
			in_request();
			do_responsemult();
		}
		else if(stricmp(c,strp)){
		    do_print_info();
		}
        else if(stricmp(c,stra)){
		    do_print_actMem();
		}
        else if(stricmp(c,strx)){
		    do_print_auxMem();
		}
		else if(stricmp(c,stre)){
		    break;
		}
		else {
			do_request();
			do_responsemult();
		}
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
