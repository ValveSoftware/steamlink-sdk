#define MAX_NAME_LENGTH 100

#define SMB_S_OK 0x000
#define E_SMB_NO_WORKGROUP_FOUND 0x001
#define E_SMB_NO_MACHINE_FOUND 0x002
#define E_SMB_NO_SHARE_FOUND 0x003
#define E_SMB_CANNOT_MOUNT 0x004
#define INT32 long

struct smb_name_list {
        struct smb_name_list *prev, *next;
        char *name, *comment;
        INT32 server_type;
};

void Samba_Initialize();

INT32 Samba_GetWorkgroupList(struct smb_name_list *workgroup_list);

INT32 Samba_GetMachineList(struct smb_name_list *machine_list, char *workgroup_name);

INT32 Samba_GetShareList(struct smb_name_list *share_list, char *machine_name);

INT32 Samba_Mount(char *machine_name, char *share_name, char *mount_point, char *user_name, char *password);

void Samba_Free_Name_List(struct smb_name_list *name_list);

void Samba_Deinitialize();
