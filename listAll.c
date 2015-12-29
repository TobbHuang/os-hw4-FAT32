#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

int read_n_bytes(int fp, void *dest, int offset, int num_bytes);
void readFAT(unsigned char* buffer,int start,int size,int fp);
int readData(unsigned char* source,int size);
void hexDump(void *source, int size, int start_offset);
void printName(int fp,int offset,char* head);

int bytesPerSector,sectorsPerCluster,reservedRegionSizeSectors,numberCopyOfFileAlloctionTables,numberOfDirectoryEntries,sizeOfFATSectors,clusterNumberOfRootDirectory,startOfDataregion;

int main(int argc, const char * argv[]) {
    
    int fp, start, size;
    unsigned char *buffer;
    
    /** OPEN the looper or images using System API */
    /** REMIND: you will need O_NONBLOCK as flag */
    if ((fp = open (argv[1],O_NONBLOCK ) ) == -1)
    {
        perror("Error opening image");
        //free(buffer);
        exit(-1);
    }
    
    // 从Boot Record里读取各种信息
    start=0;
    size=512;
    buffer = (unsigned char*) malloc(size*sizeof(unsigned char)+9);
    readFAT(buffer,start, size, fp);
    
    bytesPerSector=readData(&buffer[11], 2);
    sectorsPerCluster=readData(&buffer[13], 1);
    reservedRegionSizeSectors=readData(&buffer[14], 2);
    numberCopyOfFileAlloctionTables=readData(&buffer[16], 1);
    numberOfDirectoryEntries=readData(&buffer[17], 2);
    sizeOfFATSectors=readData(&buffer[36], 4);
    clusterNumberOfRootDirectory=readData(&buffer[44], 4);
    
    // 计算data region的起始地址
    startOfDataregion = reservedRegionSizeSectors * bytesPerSector + numberCopyOfFileAlloctionTables * sizeOfFATSectors * bytesPerSector;
    
    free(buffer);
    
    printName(fp, startOfDataregion,"./");
    
    return 0;
}


int read_n_bytes(int fp, void *dest, int num_bytes, int offset)
{
    /** OFFSET FROM BEGINING OF PARTITION WHERE THE INFO RESIDES */
    if (lseek(fp,offset,SEEK_SET)==-1)
    {
        perror("Seek error");
        return -1;
    }
    
    /** READ BYTES */
    if (read(fp,dest,num_bytes)==-1)
    {
        perror("Read error");
        return -1;
    }
    
    return 0;
}

void readFAT(unsigned char* buffer,int start,int size,int fp){
    
    int end = start+size;
    end = ((end&0x0F)==0x0F)?end:(end | 0x0F);
    start = start & 0xFFFFFFF0;
    size = end - start + 1;
    
    if(buffer == NULL)
    {
        perror("Error allocating memory");
        exit(-1);
    }
    
    if(read_n_bytes(fp, buffer, size, start) == -1)
    {
        perror("Error reading image");
        free(buffer);
        exit(-1);
    }
    
}

int readData(unsigned char* source,int size){
    int count=0;
    
    int i;
    for(i=size-1;i>=0;i--){
        count=count*256+source[i];
    }
    
    return count;
}

void hexDump(void *source, int size, int start_offset)
{
    int i,j;
    int start = 0 + start_offset;
    
    unsigned char *checker = (unsigned char*)malloc(size*sizeof(unsigned char)+9);
    memcpy(checker, source, size);
    
    /************
     *  HEADER  *
     ************/
    printf("address\t");
    for(i=0; i<16; i++)
        printf("%2X ", i);
    printf(" ");
    for(i=0; i<16; i++)
        printf("%X", i);
    printf("\n");
    
    for(i=0; i<size; i++)
    {
        /********
         * DATA *
         ********/
        if(i % 16 == 0) printf("%07x\t", start+i);
        printf("%02x ", checker[i]);
        
        /********************
         * REFERENCE VALUES *
         ********************/
        if((i+1)% 16 == 0)
        {
            printf("|");
            for(j=i-15; j<=i; j++)
            {
                if(checker[j] >= 0x20 && checker[j] <= 0x7E)
                    printf("%c", checker[j]);
                else
                    printf(".");
            }
            printf("|\n");
        }
    }
    
    printf("\n");
    
    free(checker);
}

void printName(int fp,int offset,char* head){
    unsigned char* buffer;
    char name[300]={'\0'};
    
    int i=0;
    
    while(1){
        buffer = (unsigned char*) malloc(32*sizeof(unsigned char)+9);
        readFAT(buffer,offset+i*32, 32, fp);
        
        int attribute=readData(&buffer[11], 2);
        
        if(attribute==15){
            // LFN
            char tmp[14];
            tmp[0]=readData(&buffer[1], 1);
            tmp[1]=readData(&buffer[3], 1);
            tmp[2]=readData(&buffer[5], 1);
            tmp[3]=readData(&buffer[7], 1);
            tmp[4]=readData(&buffer[9], 1);
            tmp[5]=readData(&buffer[14], 1);
            tmp[6]=readData(&buffer[16], 1);
            tmp[7]=readData(&buffer[18], 1);
            tmp[8]=readData(&buffer[20], 1);
            tmp[9]=readData(&buffer[22], 1);
            tmp[10]=readData(&buffer[24], 1);
            tmp[11]=readData(&buffer[28], 1);
            tmp[12]=readData(&buffer[30], 1);
            tmp[13]='\0';
            strcat(tmp,name);
            strcpy(name,tmp);
        } else if (attribute==16){
            // 目录
            char tmp[300];
            strcpy(tmp,head);
            strcat(tmp, name);
            strcpy(name,tmp);
            printf("%s\n",name);
            
            int index1=readData(&buffer[20], 2);
            int index2=readData(&buffer[26], 2);
            int index=index1*256+index2;
            
            strcat(name, "/");
            printName(fp, (index - clusterNumberOfRootDirectory) * sectorsPerCluster * bytesPerSector + startOfDataregion+64, name);
            
            name[0]='\0';
        } else if(attribute==32){
            // 执行档
            if(name[0]!='.'&&name[strlen(name)-1]!='~'&&name[0]!=0){
                // 不显示点文件 和 最后一个字符是~的文件（我也不知道那是什么鬼。。） 和 空文件名
                char tmp[300];
                strcpy(tmp,head);
                strcat(tmp, name);
                strcpy(name,tmp);
                printf("%s\n",name);
            }
            name[0]='\0';
        }
        else{
            // 结束了
            break;
        }
        
        free(buffer);
        i++;
        
    }
    
    free(buffer);
    
}

