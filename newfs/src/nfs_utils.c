#include "../include/nfs.h"

extern struct nfs_super      super; 
extern struct custom_options nfs_options;

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* nfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int nfs_calc_lvl(const char * path) {
    char* str = path;
    int   lvl = 0;
    //path是根目录
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    //遍历字符，遇到/层级+1
    while (*str) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int nfs_driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = NFS_ROUND_DOWN(offset, NFS_BLOCK_SZ());       //要读取的数据段和512B对齐的下界down
    int      bias           = offset - offset_aligned;                      //偏移量和数据块对齐后的偏移量之间的偏差
    int      size_aligned   = NFS_ROUND_UP((size + bias), NFS_BLOCK_SZ());  //将所需读取的数据大小和偏差对齐到数据块后的大小
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);                   //移动磁盘头
    //用while循环每次读IO大小的数据
    while (size_aligned != 0)
    {
        ddriver_read(NFS_DRIVER(), (char*)cur, NFS_IO_SZ());                
        cur          += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int nfs_driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = NFS_ROUND_DOWN(offset, NFS_BLOCK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NFS_ROUND_UP((size + bias), NFS_BLOCK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    //先读出数据
    nfs_driver_read(offset_aligned, temp_content, size_aligned);
    //将内容写入缓冲区
    memcpy(temp_content + bias, in_content, size);
    //移动磁盘头
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    //用while循环每次写IO大小的数据
    while (size_aligned != 0)
    {
        ddriver_write(NFS_DRIVER(), (char*)cur, NFS_IO_SZ());
        cur          += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();   
    }

    free(temp_content);
    return NFS_ERROR_NONE;
}
/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int nfs_alloc_dentry(struct nfs_inode* inode, struct nfs_dentry* dentry) {
    //还没有任何目录项
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
        int is_find_free_entry;
        int ino_cursor = 0;
        int byte_cursor, bit_cursor;
        for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(super.map_data_blks); byte_cursor++)
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                    super.map_data[byte_cursor] |= (0x1 << bit_cursor);//将其置为1
                    is_find_free_entry = TRUE;           
                    break;
                }
                ino_cursor++;
            }
            if (is_find_free_entry) {
                break;
            }
        }
        //遍历完数据块位图，没找到空闲的inode，返回error
        if(!is_find_free_entry || ino_cursor == super.data_num){
            return -NFS_ERROR_NOSPACE;
        }
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return nfs_inode
 */
struct nfs_inode* nfs_alloc_inode(struct nfs_dentry * dentry) {
    struct nfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    // int data_cursor = 0;
    boolean is_find_free_entry = FALSE;
    // boolean is_find_free_entry_blocks = FALSE;
    //索引节点位图上找空闲的索引节点
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(super.map_inode_blks); byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                super.map_inode[byte_cursor] |= (0x1 << bit_cursor);//将其置为1
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == super.max_ino)
        return (struct nfs_inode*)-NFS_ERROR_NOSPACE;

    inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;

    /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
    /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    return inode;
}

/**
 * @brief 分配数据块，占用位图
 * 
 * @param inode  指向inode
 * @return int
 */
int nfs_alloc_data(struct nfs_inode* inode) {
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int data_cursor = 0;
    boolean is_find_free_entry = FALSE;
    
    //数据块位图上找空闲的数据块
    for(byte_cursor = 0;byte_cursor < (NFS_BLKS_SZ(super.map_data_blks));byte_cursor++){
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0){
                // super.map_data[byte_cursor] |= (0x1 << bit_cursor);//将其置为1
                inode->block_index[data_cursor] = byte_cursor * UINT8_BITS + bit_cursor;//磁盘块号
                data_cursor ++;
            }
            if(data_cursor == NFS_DATA_PER_FILE && byte_cursor * UINT8_BITS + bit_cursor < DATA_BLOCK_NUM){
                is_find_free_entry = TRUE;
                break;
            }
        }
        if(is_find_free_entry){
            break;
        }
    }
    if(!is_find_free_entry){
        return -NFS_ERROR_NOSPACE;
    }

    if (NFS_IS_REG(inode)) {
        for(int i=0;i < NFS_DATA_PER_FILE;i++){
            inode->block_pointer[i] = (uint8_t *)malloc(NFS_BLOCK_SZ());  
        }
    }
    return 0;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int nfs_sync_inode(struct nfs_inode * inode) {
    struct nfs_inode_d  inode_d;
    struct nfs_dentry*  dentry_cursor;
    struct nfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;

    for(int i=0;i < NFS_DATA_PER_FILE;i++){
        inode_d.block_pointer[i] = inode->block_index[i];
    }
    if (nfs_driver_write(NFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return -NFS_ERROR_IO;
    }
                                                      /* Cycle 1: 写 INODE */
                                                       /* Cycle 2: 写 数据 */
    if (NFS_IS_DIR(inode)) {
        int counter = 0;                          
        dentry_cursor = inode->dentrys;
        
        while (dentry_cursor != NULL && counter < NFS_DATA_PER_FILE)
        {
            int offset = NFS_DATA_OFS(inode->block_index[counter]);
            while(dentry_cursor){
                memcpy(dentry_d.fname, dentry_cursor->fname, MAX_NAME_LEN);
                dentry_d.ftype = dentry_cursor->ftype;
                dentry_d.ino = dentry_cursor->ino;
                if (nfs_driver_write(offset, (uint8_t *)&dentry_d, sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE) {
                    NFS_DBG("[%s] io error\n", __func__);
                    return -NFS_ERROR_IO;                     
                }
            
                if (dentry_cursor->inode) {
                    nfs_sync_inode(dentry_cursor->inode);
                }

                dentry_cursor = dentry_cursor->brother;
                offset += sizeof(struct nfs_dentry_d);
            }
            counter ++;
        }
    }
    else if (NFS_IS_REG(inode)) {
        for(int i=0;i<NFS_DATA_PER_FILE;i++){
            if (nfs_driver_write(NFS_DATA_OFS(inode->block_index[i]), inode->block_pointer[i], 
                             NFS_BLOCK_SZ()) != NFS_ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return -NFS_ERROR_IO;
            }
        }  
    }
    return NFS_ERROR_NONE;
}

/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct nfs_inode* 
 */
struct nfs_inode* nfs_read_inode(struct nfs_dentry * dentry, int ino) {
    struct nfs_inode* inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));
    struct nfs_inode_d inode_d;
    struct nfs_dentry* sub_dentry;
    struct nfs_dentry_d dentry_d;
    int    dir_cnt = 0, i;
    if (nfs_driver_read(NFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;

    for(i=0;i<NFS_DATA_PER_FILE;i++){
        inode->block_index[i] = inode_d.block_pointer[i];
    }
    int offset;
    if (NFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        int counter = 0;
        while(dir_cnt != 0){
            offset = NFS_DATA_OFS(inode->block_index[counter]);
            while(dir_cnt > 0){
                if(nfs_driver_read(offset,(uint8_t *)&dentry_d,sizeof(struct nfs_dentry_d))!= NFS_ERROR_NONE){
                    NFS_DBG("[%s] io error\n", __func__);
                    return NULL;
                }
                sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
                sub_dentry->parent = inode->dentry;
                sub_dentry->ino    = dentry_d.ino; 
                nfs_alloc_dentry(inode, sub_dentry);
                offset += sizeof(struct nfs_dentry_d);
                dir_cnt -- ;
            }
            counter ++;
        }
    }
    else if (NFS_IS_REG(inode)) {
        for(int i=0;i < NFS_DATA_PER_FILE;i++){
            inode->block_pointer[i] = (uint8_t *)malloc(NFS_BLOCK_SZ());
            if (nfs_driver_read(NFS_DATA_OFS(inode->block_index[i]), (uint8_t *)inode->block_pointer[i], 
                            NFS_BLOCK_SZ()) != NFS_ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;     
            }        
        }
    }
    return inode;
}
/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct nfs_dentry* 
 */
struct nfs_dentry* nfs_get_dentry(struct nfs_inode * inode, int dir) {
    struct nfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief 
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct nfs_inode* 
 */
struct nfs_dentry* nfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct nfs_dentry* dentry_cursor = super.root_dentry;
    struct nfs_dentry* dentry_ret = NULL;
    struct nfs_inode*  inode; 
    int   total_lvl = nfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = super.root_dentry;
    }
    //按 / 分割路径
    fname = strtok(path_cpy, "/");
    //逐个处理文件名       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            nfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;
        
        if (NFS_IS_REG(inode) && lvl < total_lvl) {
            NFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        
        if (NFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
                NFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = nfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}
/**
 * @brief 挂载sfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data |
 * 
 * IO_SZ = BLK_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int nfs_mount(struct custom_options options){
    int                 ret = NFS_ERROR_NONE;
    int                 driver_fd;
    struct nfs_super_d  nfs_super_d; 
    struct nfs_dentry*  root_dentry;
    struct nfs_inode*   root_inode;

    int                 inode_num;      //索引节点
    int                 data_num;       //数据块
    int                 map_inode_blks; //索引节点块位图
    int                 map_data_blks;  //数据块位图
    int                 super_blks;     //超级块
    boolean             is_init = FALSE;

    super.is_mounted = FALSE;

    driver_fd = ddriver_open((char*)options.device);
    if (driver_fd < 0) {
        return driver_fd;
    }

    super.fd = driver_fd;
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &super.sz_disk);
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &super.sz_io);
    super.sz_blks = super.sz_io * 2;

    root_dentry = new_dentry("/", NFS_DIR_FILE);

    if (nfs_driver_read(NFS_SUPER_OFS, (uint8_t *)(&nfs_super_d), 
                        sizeof(struct nfs_super_d)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }   
                                                      /* 读取super */
    if (nfs_super_d.magic != NFS_MAGIC) {     /* 幻数无 */
                                                      /* 估算各部分大小 */
        super_blks = SUPER_BLOCK_NUM;
        map_inode_blks = INODE_MAP_BLOCK_NUM;
        map_data_blks = DATA_MAP_BLOCK_NUM;
        inode_num = INODE_BLOCK_NUM;
        data_num = DATA_BLOCK_NUM;
                                                      /* 布局layout */
        super.max_ino = inode_num;
        super.data_num = data_num;
        super.magic = NFS_MAGIC;

        nfs_super_d.map_inode_offset = NFS_SUPER_OFS + NFS_BLKS_SZ(super_blks);
        nfs_super_d.map_data_offset = nfs_super_d.map_inode_offset + NFS_BLKS_SZ(map_inode_blks);
        nfs_super_d.inode_offset = nfs_super_d.map_data_offset + NFS_BLKS_SZ(map_data_blks);
        nfs_super_d.data_offset = nfs_super_d.inode_offset + super.max_ino * NFS_BLKS_SZ(inode_num);
        
        nfs_super_d.map_inode_blks  = map_inode_blks;
        nfs_super_d.map_data_blks = map_data_blks;

        nfs_super_d.sz_usage    = 0;
        NFS_DBG("inode map blocks: %d\n", map_inode_blks);
        is_init = TRUE;
    }
    super.sz_usage   = nfs_super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    super.map_inode = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_inode_blks));
    super.map_inode_blks = nfs_super_d.map_inode_blks;
    super.map_inode_offset = nfs_super_d.map_inode_offset;

    super.map_data = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_data_blks));
    super.map_data_blks = nfs_super_d.map_data_blks;
    super.map_data_offset = nfs_super_d.map_data_offset;
    
    super.inode_offset = nfs_super_d.inode_offset;
    super.data_offset = nfs_super_d.data_offset;

    if (nfs_driver_read(nfs_super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                        NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    } 
    if (nfs_driver_read(nfs_super_d.map_data_offset, (uint8_t *)(super.map_data), 
                        NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    } 

    if (is_init) {                                    /* 分配根节点 */
        root_inode = nfs_alloc_inode(root_dentry);
        nfs_sync_inode(root_inode);
    }
    
    root_inode            = nfs_read_inode(root_dentry, NFS_ROOT_INO);
    root_dentry->inode    = root_inode;
    super.root_dentry = root_dentry;
    super.is_mounted  = TRUE;
    return ret;
}
/**
 * @brief 卸载
 * 
 * @return int 
 */
int nfs_umount() {
    struct nfs_super_d  nfs_super_d; 

    if (!super.is_mounted) {
        return NFS_ERROR_NONE;
    }

    nfs_sync_inode(super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    nfs_super_d.magic               = NFS_MAGIC;
    nfs_super_d.map_inode_blks      = super.map_inode_blks;
    nfs_super_d.map_data_blks       = super.map_data_blks;

    nfs_super_d.map_inode_offset    = super.map_inode_offset;
    nfs_super_d.map_data_offset     = super.map_data_offset;

    nfs_super_d.inode_offset        = super.inode_offset;
    nfs_super_d.data_offset         = super.data_offset;
    nfs_super_d.sz_usage            = super.sz_usage;

    if (nfs_driver_write(NFS_SUPER_OFS, (uint8_t *)&nfs_super_d, 
                     sizeof(struct nfs_super_d)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }

    if (nfs_driver_write(nfs_super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                         NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }
    if (nfs_driver_write(nfs_super_d.map_data_offset, (uint8_t *)(super.map_data), 
                         NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }

    free(super.map_inode);
    free(super.map_data);

    ddriver_close(NFS_DRIVER());

    return NFS_ERROR_NONE;
}

