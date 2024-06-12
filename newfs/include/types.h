#ifndef _TYPES_H_
#define _TYPES_H_

/******************************************************************************
* SECTION: Type def
*******************************************************************************/
typedef int         boolean;
typedef uint16_t    flag16;

typedef enum nfs_file_type{
    NFS_REG_FILE,   //普通文件
    NFS_DIR_FILE    //目录
}NFS_FILE_TYPE;
/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define NEWFS_DEFAULT_PERM        0777

#define MAX_NAME_LEN            128
#define NFS_INODE_PER_FILE      1
#define NFS_DATA_PER_FILE       6

#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define SUPER_BLOCK_NUM         1
#define INODE_MAP_BLOCK_NUM     1
#define DATA_MAP_BLOCK_NUM      1
#define INODE_BLOCK_NUM         585
#define DATA_BLOCK_NUM          3508

#define NFS_SUPER_OFS           0
#define NFS_ROOT_INO            0
#define SEEK_SET                0

#define NFS_ERROR_NONE          0
#define NFS_ERROR_ACCESS        EACCES
#define NFS_ERROR_SEEK          ESPIPE     
#define NFS_ERROR_ISDIR         EISDIR
#define NFS_ERROR_NOSPACE       ENOSPC
#define NFS_ERROR_EXISTS        EEXIST
#define NFS_ERROR_NOTFOUND      ENOENT
#define NFS_ERROR_UNSUPPORTED   ENXIO
#define NFS_ERROR_IO            EIO     /* Error Input/Output */
#define NFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define NFS_IO_SZ()                     (super.sz_io)
#define NFS_DISK_SZ()                   (super.sz_disk)
#define NFS_DRIVER()                    (super.fd)
#define NFS_BLOCK_SZ()                  (super.sz_blks)

#define NFS_ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)      //向下取整
#define NFS_ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)  //向上取整

#define NFS_BLKS_SZ(blks)               (blks * NFS_BLOCK_SZ())
#define NFS_ASSIGN_FNAME(pnfs_dentry, _fname) memcpy(pnfs_dentry->fname, _fname, strlen(_fname))

#define NFS_INO_OFS(ino)                (super.inode_offset + ino * NFS_BLOCK_SZ())     //ino索引的偏移量
#define NFS_DATA_OFS(data)              (super.data_offset + data * NFS_BLOCK_SZ())     //数据data的偏移量
#define NFS_IS_DIR(pinode)              (pinode->dentry->ftype == NFS_DIR_FILE)
#define NFS_IS_REG(pinode)              (pinode->dentry->ftype == NFS_REG_FILE)

struct nfs_dentry;
struct nfs_inode;
struct nfs_super;

struct custom_options {
	const char*        device;
};



struct nfs_super{
    uint32_t magic;
    int      fd;
    /* TODO: Define yourself */
    int      sz_io;         /* 磁盘IO大小，单位B */
    int      sz_disk;       /* 磁盘容量大小，单位B */
    int      sz_blks;       /* 逻辑块大小，单位B */
    int      sz_usage;
    int      max_ino;       /*最大支持inode数*/
    int      data_num;      /*数据块数量*/
    uint8_t* map_inode;     /*索引节点位图*/
    uint8_t* map_data;      /*数据块位图*/

    int      map_inode_blks;    /*索引节点位图于磁盘中的块数*/
    int      map_inode_offset;  /*索引节点位图于磁盘中的偏移*/

    int      map_data_blks;     /*数据块位图于磁盘中的块数*/
    int      map_data_offset;   /*数据块位图于磁盘中的偏移*/

    int      inode_offset;      /*索引节点区于磁盘中的偏移*/
    int      data_offset;       /*数据块区于磁盘中的偏移*/

    int      is_mounted;

    struct nfs_dentry* root_dentry;/*根目录对应的dentry*/
};
struct nfs_super_d
{
    uint32_t magic;
    int      sz_blks;           /* 逻辑块大小，单位B */
    int      sz_usage;          /* 逻辑块大小，单位B */
    int      max_ino;           /*最大支持inode数*/
    int      map_inode_blks;    /*索引节点位图于磁盘中的块数*/
    int      map_inode_offset;  /*数据块位图于磁盘中的偏移*/
    int      map_data_blks;     /*数据块位图于磁盘中的块数*/
    int      map_data_offset;   /*数据块位图于磁盘中的偏移*/
    int      inode_offset;      /*索引节点区于磁盘中的偏移*/
    int      data_offset;       /*数据块区于磁盘中的偏移*/
};


struct nfs_inode {
    uint32_t ino;
    /* TODO: Define yourself */
    int     size;
    int     dir_cnt;             //目录项个数
    struct nfs_dentry* dentry;
    struct nfs_dentry* dentrys;  /*所有目录项*/
    int     block_index[6];      //数据块在磁盘中的块号
    uint8_t* block_pointer[6];   //数据块指针
};

struct nfs_inode_d
{
    int     ino;                //在inode位图中的下标
    int     size;               //文件已占用空间
    int     link;               //链接数，默认为1
    NFS_FILE_TYPE ftype;        //文件类型
    int     block_pointer[6];   //数据块指针
    int     dir_cnt;            //如果是目录类型文件，下面有几个目录项
};

struct nfs_dentry {
    char     fname[MAX_NAME_LEN];
    uint32_t ino;
    /* TODO: Define yourself */
    NFS_FILE_TYPE ftype;
    struct nfs_dentry* parent;
    struct nfs_dentry* brother;
    struct nfs_inode*  inode;
};

struct nfs_dentry_d
{
    uint32_t ino;
    char     fname[MAX_NAME_LEN];
    NFS_FILE_TYPE ftype;
};
//在内存中创建一个新的 nfs_dentry 结构体实例，并初始化其各个成员。
static inline struct nfs_dentry* new_dentry(char * fname, NFS_FILE_TYPE ftype) {
    struct nfs_dentry * dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    NFS_ASSIGN_FNAME(dentry,fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;     
    return dentry;                              
}
 
#endif /* _TYPES_H_ */
