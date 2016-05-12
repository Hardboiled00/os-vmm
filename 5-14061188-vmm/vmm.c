#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "vmm.h"


/* ҳ�� */
PageTableItem pageTable[PAGE_SUM];
MasterPageTableItem masterPageTable[MASTER_PAGE_SUM];
/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* FIFO */
int fd;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq;



/* ��ʼ������ */
void do_init()
{
	int i, j;
	srandom(time(NULL));
    for (i = 0; i < MASTER_PAGE_SUM; i++)
    {
        masterPageTable[i].pages = pageTable + i * PAGE_SUM / MASTER_PAGE_SUM;
    }
	for (i = 0; i < PAGE_SUM; i++)
	{
		pageTable[i].pageNum = i;
		pageTable[i].filled = FALSE;
		pageTable[i].edited = FALSE;
        pageTable[i].count = 0;
		/* ʹ����������ø�ҳ�ı������� */
		switch (random() % 7)
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
		if (random() % 2 == 0)
		{
			do_page_in(&pageTable[j], j);
			pageTable[j].blockNum = j;
			pageTable[j].filled = TRUE;
			blockStatus[j] = TRUE;
            pageTable[j].count += 128;
		} else {
			blockStatus[j] = FALSE;
        }
	}
}

/* ��Ӧ���� */
void do_response()
{
    Ptr_PageTableItem ptr_pageTabIt;
    unsigned int masterPageNum, pageNum, offAddr;
    unsigned int actAddr;
    int i;
    int count;
    while (TRUE)
    {
        if ((count = read(fd, ptr_memAccReq, sizeof(MemoryAccessRequest))) < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("���ζ�ȡ����.\n");
                return;
            } else {
                printf("read FIFO failed.\n");
                exit(1);
            }
        } else if (count < sizeof(MemoryAccessRequest)) {
            printf("���ζ�ȡ����.\n");
            return;
        }
        if (ptr_memAccReq->virAddr >= ptr_memAccReq->id * VIRTUAL_MEMORY_EACH && ptr_memAccReq->virAddr < (ptr_memAccReq->id + 1) * VIRTUAL_MEMORY_EACH)
        {
            switch(ptr_memAccReq->reqType)
            {
                case 0: //������
                    {
                        printf("��������\nid��%d\t��ַ��%u\t���ͣ���ȡ\n",ptr_memAccReq->id, ptr_memAccReq->virAddr);
                        break;
                    }
                case 1: //д����
                    {
                        if (ptr_memAccReq->value >= 0 && ptr_memAccReq->value < 0xFFu)
                        {
                            printf("��������\nid��%d\t��ַ��%u\t���ͣ�д��\tֵ��%02X\n", ptr_memAccReq->id, ptr_memAccReq->virAddr, ptr_memAccReq->value);
                        } else {
                            do_error(ERROR_INVALID_REQUEST);
                        }
                        break;
                    }
                case 2:
                    {
                        printf("��������\nid��%d\t��ַ��%u\t���ͣ�ִ��\n", ptr_memAccReq->id, ptr_memAccReq->virAddr);
                        break;
                    }
                default:
                    do_error(ERROR_INVALID_REQUEST);
                    continue;
            }
        } else {
            do_error(ERROR_OVER_BOUNDARY);
            continue;
        }

        /* ����ҳ�ź�ҳ��ƫ��ֵ */
        masterPageNum = ptr_memAccReq->virAddr / PAGE_SUM;
        pageNum = ptr_memAccReq->virAddr % PAGE_SUM / PAGE_SIZE;
        offAddr = ptr_memAccReq->virAddr % PAGE_SUM % PAGE_SIZE;
        printf("һ��ҳ��Ϊ��%u\t����ҳ��Ϊ��%u\tҳ��ƫ��Ϊ��%u\n", masterPageNum, pageNum, offAddr);

        /* ��ȡ��Ӧҳ���� */
        ptr_pageTabIt = masterPageTable[masterPageNum].pages + pageNum;

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
                    if (!(ptr_pageTabIt->proType & READABLE)) //ҳ�治�ɶ�
                    {
                        do_error(ERROR_READ_DENY);
                        continue;
                    }
                    /* ��ȡʵ���е����� */
                    for (i = 0; i < PAGE_SUM; i++) 
                    {
                        pageTable[i].count /= 2;
                        if (pageTable + i == ptr_pageTabIt) 
                        {
                            pageTable[i].count += 128;
                        }
                    }
                    printf("�������ɹ���ֵΪ%02X\n", actMem[actAddr]);
                    break;
                }
            case REQUEST_WRITE: //д����
                {
                    if (!(ptr_pageTabIt->proType & WRITABLE)) //ҳ�治��д
                    {
                        do_error(ERROR_WRITE_DENY);	
                        continue;
                    }
                    /* ��ʵ����д����������� */
                    for (i = 0; i < PAGE_SUM; i++) 
                    {
                        pageTable[i].count /= 2;
                        if (pageTable + i == ptr_pageTabIt) 
                        {
                            pageTable[i].count += 128;
                        }
                    }
                    actMem[actAddr] = ptr_memAccReq->value;
                    ptr_pageTabIt->edited = TRUE;			
                    printf("д�����ɹ�\n");
                    break;
                }
            case REQUEST_EXECUTE: //ִ������
                {
                    if (!(ptr_pageTabIt->proType & EXECUTABLE)) //ҳ�治��ִ��
                    {
                        do_error(ERROR_EXECUTE_DENY);
                        continue;
                    }
                    for (i = 0; i < PAGE_SUM; i++) 
                    {
                        pageTable[i].count /= 2;
                        if (pageTable + i == ptr_pageTabIt) 
                        {
                            pageTable[i].count += 128;
                        }
                    }
                    printf("ִ�гɹ�\n");
                    break;
                }
            default: //�Ƿ���������
                {	
                    do_error(ERROR_INVALID_REQUEST);
                    continue;
                }
        }
    }
}

/* ����ȱҳ�ж� */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
    unsigned int i;
    printf("����ȱҳ�жϣ���ʼ���е�ҳ...\n");
    for (i = 0; i < PAGE_SUM; i++) 
    {
        pageTable[i].count /= 2;
        if (pageTable + i == ptr_pageTabIt) 
        {
            pageTable[i].count += 128;
        }
    }
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
			blockStatus[i] = TRUE;
            return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
    do_switch(ptr_pageTabIt);
}

void do_switch(Ptr_PageTableItem ptr_pageTabIt)
{
    unsigned int rec = 0, i;
    unsigned char min = 255;
	printf("û�п�������飬��ʼ�����ϻ�ҳ���滻...\n");
    for (i = 0; i < PAGE_SUM; i++)
    {
        if (pageTable[i].filled && pageTable[i].count < min){
            min = pageTable[i].count;
            rec = i;
        }
    }
    printf("ѡ���%uҳ�����滻\n", rec);
    if (pageTable[rec].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(pageTable + rec);
	}
	pageTable[rec].filled = FALSE;
	pageTable[rec].count = 0;

	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[rec].blockNum);
	
	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[rec].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
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
		printf("DEBUG: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
        printf("actMem:%d\tblockNum:%d\n", actMem, blockNum);
        printf("readNum:%d\n", readNum);
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
        case ERROR_FILE_CREATE_FAILED:
        {
            printf("ϵͳ���󣺴����ļ�ʧ��\n");
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
	printf("ҳ��\t���\tװ��\t�޸�\t����\t����\t����\n");
	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i, pageTable[i].blockNum, pageTable[i].filled, pageTable[i].edited, get_proType_str(str, pageTable[i].proType), pageTable[i].count, pageTable[i].auxAddr);
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

void initFile()
{
    int i;
    char *key = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char buffer[VIRTUAL_MEMORY_SIZE + 1];
    
    ptr_auxMem = fopen(AUXILIARY_MEMORY, "w");
    for (i = 0; i < VIRTUAL_MEMORY_SIZE; i++) 
    {
        buffer[i] = key[rand() % 62];
    }
    buffer[VIRTUAL_MEMORY_SIZE] = '\0';
    fwrite(buffer, sizeof(BYTE), VIRTUAL_MEMORY_SIZE, ptr_auxMem);
	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
}


int main(int argc, char* argv[])
{
	char c;
	int i;
    struct stat statbuf;

    if (access(AUXILIARY_MEMORY, F_OK) == -1)
    {
        if(creat(AUXILIARY_MEMORY, 0755) < 0)
        {
            do_error(ERROR_FILE_CREATE_FAILED);
        }
        initFile();
    }
    if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
    {
        do_error(ERROR_FILE_OPEN_FAILED);
        exit(1);
    }

    if(stat(FIFO, &statbuf)==0)
    {
        /* ���FIFO�ļ�����,ɾ�� */
        if(remove(FIFO)<0)
        {
            printf("remove FIFO failed");
            exit(1);
        }
    }
    if(mkfifo(FIFO, 0666)<0)
    {
        printf("mkfifo failed");
        exit(1);
    }

    /* �ڷ�����ģʽ�´�FIFO */
    if((fd = open(FIFO, O_RDONLY | O_NONBLOCK)) < 0) 
    {
        printf("open FIFO failed");
        exit(1);
    }

	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* ��ѭ����ģ��ô������봦����� */
	while (TRUE)
	{
        printf("��S��ȡ���󣬰�����������ȡ����...\n");
        if ((c = getchar()) == 's' || c == 'S')
            do_response();
        while (c != '\n')
            c = getchar();
		printf("��Y��ӡҳ��������������ӡ...\n");
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
		while (c != '\n')
			c = getchar();
		printf("��X�˳����򣬰�����������...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
	}
    close(fd);
	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
