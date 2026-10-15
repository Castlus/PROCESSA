/* =====================================================================
 *  PROCESSA - carregador UEFI (\EFI\BOOT\BOOTX64.EFI)
 * ---------------------------------------------------------------------
 *  PCs sem "Legacy/CSM" so dao boot em UEFI. O firmware UEFI sabe ler um
 *  pendrive FAT32 e executa \EFI\BOOT\BOOTX64.EFI, que e este programa
 *  (um executavel PE32+, o mesmo formato de um .exe do Windows). Ele:
 *    1. le \PROCESSA\KERNEL.BIN do pendrive para 0x100000;
 *    2. le os programas de \PROCESSA\INICIO (iniciam no boot) e de
 *       \PROCESSA\PROGRAMAS (rodam com "run") e monta com eles, na memoria,
 *       um disco PROCFS: o mesmo formato que o kernel le no QEMU;
 *    3. pega a tela grafica (GOP), mede a frequencia do TSC da CPU e le o
 *       mapa de memoria;
 *    4. chama ExitBootServices (o firmware sai de cena) e salta para o
 *       kernel, ja em 64 bits, com as informacoes em RDI.
 *  Escrito do zero: so as estruturas do UEFI que usamos, sem gnu-efi/EDK2.
 * ===================================================================== */
#include "bootinfo.h"

typedef unsigned long long UINT64;
typedef unsigned long long UINTN;
typedef unsigned int       UINT32;
typedef unsigned short     UINT16;
typedef unsigned char      UINT8;
typedef unsigned short     CHAR16;
typedef UINT64             EFI_STATUS;
typedef void              *EFI_HANDLE;
typedef UINT64             EFI_PHYSICAL_ADDRESS;

#define EFI_ERROR(s)        ((long long)(s) < 0)
#define EFI_LOAD_ERROR      0x8000000000000001ull

#define ALLOCATE_MAX_ADDRESS 1
#define ALLOCATE_ADDRESS     2
#define EFI_LOADER_DATA      2

#define EFI_FILE_MODE_READ   1
#define EFI_FILE_DIRECTORY   0x10

#define FS_LBA          2048        /* mesmo layout de tools/mkdisk.py */
#define DIR_SECTORS     8
#define SECTOR          512
#define MAX_FILES       63
#define FLAG_AUTORUN    1
#define KERNEL_PAGES    256         /* 1 MiB para o kernel + .bss */
#define RAMDISK_PAGES   2048        /* ate 8 MiB de programas */
#define BELOW_1GIB      0x3FFFFFFFull   /* o kernel so enxerga 0-1 GiB de memoria */

typedef struct {
    UINT32 d1;
    UINT16 d2, d3;
    UINT8  d4[8];
} EFI_GUID;

typedef struct {
    UINT64 Signature;
    UINT32 Revision, HeaderSize, CRC32, Reserved;
} EFI_TABLE_HEADER;

typedef struct SIMPLE_TEXT_OUTPUT {
    void *Reset;
    EFI_STATUS (*OutputString)(struct SIMPLE_TEXT_OUTPUT *self, CHAR16 *s);
    void *TestString, *QueryMode, *SetMode, *SetAttribute;
    EFI_STATUS (*ClearScreen)(struct SIMPLE_TEXT_OUTPUT *self);
    void *SetCursorPosition, *EnableCursor, *Mode;
} SIMPLE_TEXT_OUTPUT;

typedef struct {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    UINT64 VirtualStart, NumberOfPages, Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL, *RestoreTPL;
    EFI_STATUS (*AllocatePages)(UINT32 type, UINT32 memtype, UINTN pages, EFI_PHYSICAL_ADDRESS *addr);
    void *FreePages;
    EFI_STATUS (*GetMemoryMap)(UINTN *size, EFI_MEMORY_DESCRIPTOR *map, UINTN *key,
                               UINTN *desc_size, UINT32 *desc_version);
    void *AllocatePool, *FreePool;
    void *CreateEvent, *SetTimer, *WaitForEvent, *SignalEvent, *CloseEvent, *CheckEvent;
    void *InstallProtocolInterface, *ReinstallProtocolInterface, *UninstallProtocolInterface;
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE handle, EFI_GUID *guid, void **iface);
    void *Reserved, *RegisterProtocolNotify, *LocateHandle, *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage, *StartImage, *Exit, *UnloadImage;
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE image, UINTN map_key);
    void *GetNextMonotonicCount;
    EFI_STATUS (*Stall)(UINTN microseconds);
    EFI_STATUS (*SetWatchdogTimer)(UINTN timeout, UINT64 code, UINTN size, CHAR16 *data);
    void *ConnectController, *DisconnectController;
    void *OpenProtocol, *CloseProtocol, *OpenProtocolInformation;
    void *ProtocolsPerHandle, *LocateHandleBuffer;
    EFI_STATUS (*LocateProtocol)(EFI_GUID *guid, void *registration, void **iface);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    SIMPLE_TEXT_OUTPUT *ConOut;
    EFI_HANDLE StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;            /* o dispositivo de onde viemos (o pendrive) */
    void *FilePath, *Reserved;
    UINT32 LoadOptionsSize;
    void *LoadOptions, *ImageBase;
    UINT64 ImageSize;
    UINT32 ImageCodeType, ImageDataType;
    void *Unload;
} EFI_LOADED_IMAGE;

typedef struct EFI_FILE {
    UINT64 Revision;
    EFI_STATUS (*Open)(struct EFI_FILE *self, struct EFI_FILE **out, CHAR16 *name,
                       UINT64 mode, UINT64 attributes);
    EFI_STATUS (*Close)(struct EFI_FILE *self);
    void *Delete;
    EFI_STATUS (*Read)(struct EFI_FILE *self, UINTN *size, void *buffer);
    void *Write, *GetPosition, *SetPosition, *GetInfo, *SetInfo, *Flush;
} EFI_FILE;

typedef struct {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(void *self, EFI_FILE **root);
} EFI_SIMPLE_FILE_SYSTEM;

typedef struct {
    UINT16 Year;
    UINT8 Month, Day, Hour, Minute, Second, Pad1;
    UINT32 Nanosecond;
    UINT16 TimeZone;
    UINT8 Daylight, Pad2;
} EFI_TIME;

typedef struct {
    UINT64 Size, FileSize, PhysicalSize;
    EFI_TIME CreateTime, LastAccessTime, ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[];
} EFI_FILE_INFO;

typedef struct {
    UINT32 Version, HorizontalResolution, VerticalResolution, PixelFormat;
    UINT32 RedMask, GreenMask, BlueMask, ReservedMask;
    UINT32 PixelsPerScanLine;
} EFI_GOP_MODE_INFO;

typedef struct {
    UINT32 MaxMode, Mode;
    EFI_GOP_MODE_INFO *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GOP_MODE;

typedef struct {
    void *QueryMode, *SetMode, *Blt;
    EFI_GOP_MODE *Mode;
} EFI_GOP;

static EFI_GUID loaded_image_guid = { 0x5B1B31A1, 0x9562, 0x11D2,
                                      { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID simple_fs_guid    = { 0x964E5B22, 0x6459, 0x11D2,
                                      { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID gop_guid          = { 0x9042A9DE, 0x23DC, 0x4A38,
                                      { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } };

static EFI_SYSTEM_TABLE *ST;
static EFI_BOOT_SERVICES *BS;

/* O gcc pode gerar chamadas a estas duas sozinho (copias de structs). */
void *memset(void *dst, int c, UINTN n)
{
    UINT8 *d = dst;
    while (n--)
        *d++ = (UINT8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, UINTN n)
{
    UINT8 *d = dst;
    const UINT8 *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

/* ---------------- texto na tela (antes de sair do firmware) ---------------- */

static void print(const char *s)
{
    CHAR16 buf[128];
    UINTN n = 0;
    while (*s) {
        if (*s == '\n')
            buf[n++] = '\r';
        buf[n++] = (CHAR16)(UINT8)*s++;
        if (n >= 120) {
            buf[n] = 0;
            ST->ConOut->OutputString(ST->ConOut, buf);
            n = 0;
        }
    }
    buf[n] = 0;
    ST->ConOut->OutputString(ST->ConOut, buf);
}

static void print_num(UINT64 v)
{
    char buf[24];
    int i = 23;
    buf[i] = 0;
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    print(&buf[i]);
}

static EFI_STATUS fail(const char *msg)
{
    print("\nPROCESSA: ");
    print(msg);
    print("\nVoltando para o firmware em 20 segundos...\n");
    BS->Stall(20 * 1000 * 1000);
    return EFI_LOAD_ERROR;
}

static UINT64 rdtsc(void)
{
    UINT32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((UINT64)hi << 32) | lo;
}

static void ascii_from_utf16(char *dst, UINTN size, const CHAR16 *src)
{
    UINTN i = 0;
    for (; src[i] && i + 1 < size; i++)
        dst[i] = src[i] < 128 ? (char)src[i] : '?';
    dst[i] = 0;
}

/* ---------------- leitura de arquivos do pendrive (FAT, pelo firmware) ---------------- */

static EFI_STATUS read_all(EFI_FILE *f, UINT8 *dst, UINTN max, UINTN *got)
{
    *got = 0;
    while (*got < max) {
        UINTN n = max - *got;
        EFI_STATUS st = f->Read(f, &n, dst + *got);
        if (EFI_ERROR(st))
            return st;
        if (n == 0)
            break;
        *got += n;
    }
    return 0;
}

struct procfs_header {
    char magic[8];
    UINT32 count;
    UINT8 reserved[52];
};

struct procfs_entry {
    char name[40];
    UINT32 lba, size, flags;
    UINT8 reserved[12];
};

struct ramdisk {
    UINT8 *base;
    UINTN used_sectors;             /* inclui o diretorio */
    struct procfs_header *header;
    struct procfs_entry *entries;
};

/* Le todos os arquivos de uma pasta do pendrive para dentro do PROCFS. */
static void load_folder(EFI_FILE *root, CHAR16 *path, UINT32 flags, struct ramdisk *rd)
{
    EFI_FILE *dir;
    if (EFI_ERROR(root->Open(root, &dir, path, EFI_FILE_MODE_READ, 0)))
        return;                     /* pasta nao existe: tudo bem */

    UINT64 info_buf[128];           /* 1 KiB, alinhado */
    EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_buf;
    for (;;) {
        UINTN size = sizeof info_buf;
        if (EFI_ERROR(dir->Read(dir, &size, info)) || size == 0)
            break;
        if (info->Attribute & EFI_FILE_DIRECTORY)
            continue;
        if (rd->header->count >= MAX_FILES) {
            print("  (limite de arquivos atingido)\n");
            break;
        }

        UINTN sectors = (info->FileSize + SECTOR - 1) / SECTOR;
        if (rd->used_sectors + sectors > RAMDISK_PAGES * 8) {
            print("  sem espaco para mais programas\n");
            break;
        }

        EFI_FILE *f;
        UINTN got;
        UINT8 *dst = rd->base + rd->used_sectors * SECTOR;
        if (EFI_ERROR(dir->Open(dir, &f, info->FileName, EFI_FILE_MODE_READ, 0)))
            continue;
        EFI_STATUS st = read_all(f, dst, info->FileSize, &got);
        f->Close(f);
        if (EFI_ERROR(st) || got != info->FileSize)
            continue;

        struct procfs_entry *e = &rd->entries[rd->header->count++];
        ascii_from_utf16(e->name, sizeof e->name, info->FileName);
        e->lba = FS_LBA + (UINT32)rd->used_sectors;
        e->size = (UINT32)info->FileSize;
        e->flags = flags;
        rd->used_sectors += sectors;

        print("  ");
        print(e->name);
        print(flags & FLAG_AUTORUN ? " (inicia no boot)\n" : "\n");
    }
    dir->Close(dir);
}

/* ---------------- ponto de entrada ---------------- */

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS st;
    ST = system_table;
    BS = ST->BootServices;
    BS->SetWatchdogTimer(0, 0, 0, 0);       /* sem reinicio automatico */

    ST->ConOut->ClearScreen(ST->ConOut);
    print("PROCESSA - carregador UEFI\n");

    /* ---- informacoes que o kernel recebe ---- */
    EFI_PHYSICAL_ADDRESS bi_addr = BELOW_1GIB;
    if (EFI_ERROR(BS->AllocatePages(ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA,
                                    (sizeof(struct uefi_bootinfo) + 4095) / 4096, &bi_addr)))
        return fail("sem memoria para as informacoes de boot");
    struct uefi_bootinfo *bi = (struct uefi_bootinfo *)bi_addr;
    memset(bi, 0, sizeof *bi);
    bi->magic = UEFI_BOOTINFO_MAGIC;
    ascii_from_utf16(bi->firmware, sizeof bi->firmware, ST->FirmwareVendor);

    /* ---- o pendrive de onde viemos ---- */
    EFI_LOADED_IMAGE *loaded;
    EFI_SIMPLE_FILE_SYSTEM *fs;
    EFI_FILE *root;
    if (EFI_ERROR(BS->HandleProtocol(image, &loaded_image_guid, (void **)&loaded)) ||
        EFI_ERROR(BS->HandleProtocol(loaded->DeviceHandle, &simple_fs_guid, (void **)&fs)) ||
        EFI_ERROR(fs->OpenVolume(fs, &root)))
        return fail("nao consegui abrir o pendrive (ele esta em FAT32?)");

    /* ---- 1. kernel em 0x100000 ---- */
    EFI_PHYSICAL_ADDRESS kaddr = UEFI_KERNEL_ADDR;
    if (EFI_ERROR(BS->AllocatePages(ALLOCATE_ADDRESS, EFI_LOADER_DATA, KERNEL_PAGES, &kaddr)))
        return fail("o endereco 0x100000 esta em uso pelo firmware");
    EFI_FILE *kfile;
    UINTN ksize;
    if (EFI_ERROR(root->Open(root, &kfile, L"\\PROCESSA\\KERNEL.BIN", EFI_FILE_MODE_READ, 0)))
        return fail("nao achei \\PROCESSA\\KERNEL.BIN no pendrive");
    st = read_all(kfile, (UINT8 *)kaddr, KERNEL_PAGES * 4096, &ksize);
    kfile->Close(kfile);
    if (EFI_ERROR(st) || ksize == 0)
        return fail("erro lendo KERNEL.BIN");
    print("kernel: ");
    print_num(ksize / 1024);
    print(" KiB em 0x100000\n");

    /* ---- 2. programas -> PROCFS na memoria ---- */
    EFI_PHYSICAL_ADDRESS rd_addr = BELOW_1GIB;
    if (EFI_ERROR(BS->AllocatePages(ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, RAMDISK_PAGES, &rd_addr)))
        return fail("sem memoria para os programas");
    struct ramdisk rd;
    rd.base = (UINT8 *)rd_addr;
    rd.used_sectors = DIR_SECTORS;
    rd.header = (struct procfs_header *)rd.base;
    rd.entries = (struct procfs_entry *)(rd.base + sizeof(struct procfs_header));
    memset(rd.base, 0, DIR_SECTORS * SECTOR);
    memcpy(rd.header->magic, "PROCFS01", 8);
    print("programas:\n");
    load_folder(root, L"\\PROCESSA\\INICIO", FLAG_AUTORUN, &rd);
    load_folder(root, L"\\PROCESSA\\PROGRAMAS", 0, &rd);
    root->Close(root);
    bi->ramdisk_addr = rd_addr;
    bi->ramdisk_sectors = rd.used_sectors;

    /* ---- 3. tela grafica ---- */
    EFI_GOP *gop;
    if (EFI_ERROR(BS->LocateProtocol(&gop_guid, 0, (void **)&gop)) || gop->Mode->Info->PixelFormat > 2)
        return fail("o firmware nao forneceu uma tela grafica (GOP)");
    EFI_GOP_MODE_INFO *mi = gop->Mode->Info;
    bi->fb_addr = gop->Mode->FrameBufferBase;
    bi->fb_width = mi->HorizontalResolution;
    bi->fb_height = mi->VerticalResolution;
    bi->fb_stride = mi->PixelsPerScanLine;
    bi->fb_format = mi->PixelFormat;
    bi->mask_r = mi->RedMask;
    bi->mask_g = mi->GreenMask;
    bi->mask_b = mi->BlueMask;
    print("tela: ");
    print_num(bi->fb_width);
    print("x");
    print_num(bi->fb_height);
    print("\n");

    /* ---- frequencia do TSC: quantos ciclos em 50 ms (o kernel usa para
     *      calibrar o timer do APIC) ---- */
    UINT64 t0 = rdtsc();
    BS->Stall(50000);
    bi->tsc_hz = (rdtsc() - t0) * 20;

    /* ---- 4. mapa de memoria + ExitBootServices ---- */
    UINTN map_pages = 16;
    EFI_PHYSICAL_ADDRESS map_addr = BELOW_1GIB;
    if (EFI_ERROR(BS->AllocatePages(ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, map_pages, &map_addr)))
        return fail("sem memoria para o mapa de memoria");
    EFI_MEMORY_DESCRIPTOR *map = (EFI_MEMORY_DESCRIPTOR *)map_addr;
    UINTN map_size, map_key, desc_size;
    UINT32 desc_version;

    print("saindo do firmware e pulando para o kernel...\n");
    st = EFI_LOAD_ERROR;
    for (int attempt = 0; attempt < 3 && EFI_ERROR(st); attempt++) {
        map_size = map_pages * 4096;
        if (EFI_ERROR(BS->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version)))
            return fail("GetMemoryMap falhou");
        st = BS->ExitBootServices(image, map_key);  /* falha se o mapa mudou: tenta de novo */
    }
    if (EFI_ERROR(st))
        return fail("ExitBootServices falhou");

    /* Daqui em diante o firmware nao existe mais: nada de print. */
    for (UINTN off = 0; off < map_size && bi->mem_count < UEFI_MEM_MAX; off += desc_size) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + off);
        /* livres agora: memoria convencional e a que era do firmware de boot */
        if (d->Type != 3 && d->Type != 4 && d->Type != 7)
            continue;
        struct uefi_mem_range *last = bi->mem_count ? &bi->mem[bi->mem_count - 1] : 0;
        if (last && last->start + last->pages * 4096 == d->PhysicalStart)
            last->pages += d->NumberOfPages;
        else
            bi->mem[bi->mem_count++] = (struct uefi_mem_range){ d->PhysicalStart, d->NumberOfPages };
    }

    /* salta para o kernel (entry.asm) com RDI = informacoes de boot */
    __asm__ volatile ("cli\n\t"
                      "mov %0, %%rdi\n\t"
                      "jmp *%1"
                      : : "r"(bi), "r"(kaddr) : "rdi", "memory");
    for (;;)
        ;
}
