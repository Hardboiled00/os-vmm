#include<time.h>
#ifndef VMM_H
#define VMM_H

#ifndef DEBUG
#define DEBUG
#endif
#undef DEBUG


/* ģ�⸨����ļ�·�� */
#define AUXILIARY_MEMORY "vmm_auxMem"

/* ҳ���С���ֽڣ�*/
#define PAGE_SIZE 4
/* ���ռ��С���ֽڣ� */
#define VIRTUAL_MEMORY_SIZE (64 * 4)

/*********************ҳ���С******************/
#define PageTableItem_SIZE 256
/*********************һ��ҳ���С**************/
#define FPage_SIZE 64
/*********************����Ҳ���С**************/
#define NPage_SIZE 4
/*********************���������**************/
#define Process_SIZE 6


/* ʵ��ռ��С���ֽڣ� */
#define ACTUAL_MEMORY_SIZE (32 * 4)
/* ����ҳ�� */
#define PAGE_SUM (VIRTUAL_MEMORY_SIZE / PAGE_SIZE)
/*****************һ��ҳ����******************/
#define FPageNum 4
/* ��������� */
#define BLOCK_SUM (ACTUAL_MEMORY_SIZE / PAGE_SIZE)


/* �ɶ���ʶλ */
#define READABLE 0x01u
/* ��д��ʶλ */
#define WRITABLE 0x02u
/* ��ִ�б�ʶλ */
#define EXECUTABLE 0x04u



/* �����ֽ����� */
#define BYTE unsigned char

typedef enum {
	TRUE = 1, FALSE = 0
} BOOL;



/* ҳ���� */
typedef struct
{
	unsigned int pageNum;
	unsigned int blockNum; //������
	BOOL filled; //ҳ��װ������λ
	BYTE proType; //ҳ�汣������
	BOOL edited; //ҳ���޸ı�ʶ
	time_t time;
	unsigned long auxAddr; //����ַ
	unsigned long count; //ҳ��ʹ�ü�����
	BOOL processNum[Process_SIZE]; //���̺�
} PageTableItem, *Ptr_PageTableItem;


/********************һ��ҳ����********************/
typedef struct{
	Ptr_PageTableItem page[16];
}FPageTableItem;



/* �ô��������� */
typedef enum {
	REQUEST_READ,
	REQUEST_WRITE,
	REQUEST_EXECUTE
} MemoryAccessRequestType;

/* �ô����� */
typedef struct
{
	MemoryAccessRequestType reqType; //�ô���������
	unsigned long virAddr; //���ַ
	BYTE value; //д�����ֵ
	unsigned int ProcessNum;//����Ľ��̺�
} MemoryAccessRequest, *Ptr_MemoryAccessRequest;


/* �ô������� */
typedef enum {
	ERROR_READ_DENY, //��ҳ���ɶ�
	ERROR_WRITE_DENY, //��ҳ����д
	ERROR_EXECUTE_DENY, //��ҳ����ִ��
	ERROR_INVALID_REQUEST, //�Ƿ���������
	ERROR_OVER_BOUNDARY, //��ַԽ��
	ERROR_FILE_OPEN_FAILED, //�ļ���ʧ��
	ERROR_FILE_CLOSE_FAILED, //�ļ��ر�ʧ��
	ERROR_FILE_SEEK_FAILED, //�ļ�ָ�붨λʧ��
	ERROR_FILE_READ_FAILED, //�ļ���ȡʧ��
	ERROR_FILE_WRITE_FAILED, //�ļ�д��ʧ��
	ERROR_PROCESS_DENY //����Ȩ��ʧ��
} ERROR_CODE;

/* �����ô����� */
void do_request();

/* ��Ӧ�ô����� */
void do_response();

/* ����ȱҳ�ж� */
void do_page_fault(Ptr_PageTableItem);

/* LFUҳ���滻 */
void do_LFU(Ptr_PageTableItem);

/*LRUҳ���滻*/
void do_LRU(Ptr_PageTableItem);

/*ҳ���ϻ��㷨*/
void do_old(Ptr_PageTableItem);

/* װ��ҳ�� */
void do_page_in(Ptr_PageTableItem, unsigned in);

/* д��ҳ�� */
void do_page_out(Ptr_PageTableItem);

/* ������ */
void do_error(ERROR_CODE);

/* ��ӡҳ�������Ϣ */
void do_print_info();

/* ��ȡҳ�汣�������ַ��� */
char *get_proType_str(char *, BYTE);

char *get_processNum_str(char *str,PageTableItem page);
#endif
