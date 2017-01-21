
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <VideoEncoderCapi.h>

static int g_num_sessions = 0;
static char g_in_filename[256];
static char g_out_filename[256];
static int g_got_out_filename = 0;
static char* g_data = 0;
static int g_in_file_bytes = 0;
static int g_width = 1024;
static int g_height = 768;

struct nv_info
{
    EncodeHandler handle;
    int frames;
    pthread_mutex_t mutex;
    int in_bytes;
    int out_bytes;
    int comp_ratio;
};

static struct nv_info g_sessions[1024];

int process_args(int argc, char** argv)
{
    int index;

    if (argc < 2)
    {
        printf("args\n");
        printf("%s -s <num sessions> "
                       "-i <input yuv file in nv12 format> "
                       "-o <output h264 file> "
                       "-w <width in pixels> "
                       "-h <height in pixels>\n", argv[0]);
        return 1;
    }
    for (index = 1; index < argc; index++)
    {
        if (strcmp(argv[index], "-s") == 0)
        {
            g_num_sessions = atoi(argv[++index]);
        }
        if (strcmp(argv[index], "-i") == 0)
        {
            strcpy(g_in_filename, argv[++index]);
        }
        if (strcmp(argv[index], "-o") == 0)
        {
            strcpy(g_out_filename, argv[++index]);
            g_got_out_filename = 1;
            unlink(g_out_filename);
        }
        if (strcmp(argv[index], "-w") == 0)
        {
            g_width = atoi(argv[++index]);
        }
        if (strcmp(argv[index], "-h") == 0)
        {
            g_height = atoi(argv[++index]);
        }
    }
    return 0;
}

/*****************************************************************************/
static int
n_save_data(const char* filename, const char* data, int data_size)
{
    int fd;

    fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        return 1;
    }
    lseek(fd, 0, SEEK_END);
    if (write(fd, data, data_size) != data_size)
    {
        printf("save_data: write failed\n");
    }
    close(fd);
    return 0;
}

void * start_routine(void* arg)
{
    VideoFrameRawData inb;
    VideoEncOutputBuffer outb;
    Encode_Status status;
    int actualDataSize;

    int index;
    int data_offset;
    char* ls8;
    char* ld8;
    struct nv_info* nvi = (struct nv_info*) arg;

    char* out_buffer = (char*) malloc(1024 * 1024);

    printf("thread started\n");
    data_offset = 0;
    nvi->frames = 0;
    while (1)
    {

        memset(&inb, 0, sizeof(inb));
        inb.memoryType = VIDEO_DATA_MEMORY_TYPE_RAW_POINTER;
        inb.width = g_width;
        inb.height = g_height;
        inb.pitch[0] = g_width;
        inb.pitch[1] = g_width;
        inb.offset[0] = 0;
        inb.offset[1] = inb.offset[0] + g_width * g_height;
        inb.fourcc = VA_FOURCC_NV12;
        inb.size = g_width * g_height * 3 / 2;
        inb.handle = (intptr_t) (g_data + data_offset);

        status = encodeEncodeRawData(nvi->handle, &inb);
        if (status != 0)
        {
            printf("error\n");
        }

        memset(&outb, 0, sizeof(outb));
        outb.data = (unsigned char *) out_buffer;
        outb.bufferSize = 1024 * 1024;
        outb.format = OUTPUT_EVERYTHING;
        status = encodeGetOutput(nvi->handle, &outb, 1);
        if (status != 0)
        {
            printf("error\n");
        }

        if (g_got_out_filename)
        {
            n_save_data(g_out_filename, outb.data, outb.dataSize);
        }

        actualDataSize = outb.dataSize;
        data_offset += g_width * g_height * 3 / 2;
        pthread_mutex_lock(&(nvi->mutex));
        nvi->frames++;
        nvi->in_bytes += g_width * g_height * 3 / 2;
        nvi->out_bytes += actualDataSize;
        if (data_offset >= g_in_file_bytes)
        {
            data_offset = 0;
            nvi->comp_ratio = nvi->in_bytes / (nvi->out_bytes + 1);
            nvi->in_bytes = 0;
            nvi->out_bytes = 0;
            g_got_out_filename = 0;
        }
        pthread_mutex_unlock(&(nvi->mutex));
    }
    printf("thread exit\n");
    return 0;
}

int main(int argc, char** argv)
{
    NativeDisplay nd;
    Encode_Status status;
    VideoParamsCommon encVideoParams;
    VideoConfigAVCStreamFormat streamFormat;
    VideoConfigBitRate bitRate1;

    int index;
    int fd;
    int rv;
    pthread_t thread;

    struct stat st;

    setlocale(LC_CTYPE, "");

    if (process_args(argc, argv) != 0)
    {
        return 0;
    }

    if (g_got_out_filename && g_num_sessions > 1)
    {
        printf("turning off -o because more tha one session requested\n");
        g_got_out_filename = 0;
    }

    memset(g_sessions, 0, sizeof(g_sessions));

    if (stat(g_in_filename, &st) == 0)
    {
        g_data = (char*) malloc(st.st_size + 16);
        if (g_data == 0)
        {
            printf("malloc failed\n");
            return 1;
        }
    }
    else
    {
        printf("problem getting in file size\n");
        return 1;
    }

    fd = open(g_in_filename, O_RDONLY);
    if (fd == -1)
    {
        printf("failed to open %s\n", g_in_filename);
        return 1;
    }
    g_in_file_bytes = read(fd, g_data, st.st_size);
    if (g_in_file_bytes < 16)
    {
        printf("read failed\n");
        return 1;
    }

    printf("read file size %d\n", g_in_file_bytes);

    printf("about to init %d sessions\n", g_num_sessions);
    for (index = 0; index < g_num_sessions; index++)
    {
        g_sessions[index].handle = createEncoder(YAMI_MIME_H264);
        if (g_sessions[index].handle == 0)
        {
            printf("session %d failed\n", index);
            return 1;
        }
        else
        {
            printf("session %d ok\n", index);
        }

        memset(&nd, 0, sizeof(nd));
        nd.type = NATIVE_DISPLAY_DRM;
        encodeSetNativeDisplay(g_sessions[index].handle, &nd);

        memset(&encVideoParams, 0, sizeof(encVideoParams));
        encVideoParams.size = sizeof(encVideoParams);
        status = encodeGetParameters(g_sessions[index].handle, VideoParamsTypeCommon, &encVideoParams);
        if (status != 0)
        {
            printf("encodeGetParameters status %d\n", status); 
        }

        encVideoParams.resolution.width = g_width;
        encVideoParams.resolution.height = g_height;
        encVideoParams.size = sizeof(encVideoParams);

        encVideoParams.rcParams.initQP = 28;
        encVideoParams.rcMode = RATE_CONTROL_CQP;
        encVideoParams.intraPeriod = -1;

        status = encodeSetParameters(g_sessions[index].handle, VideoParamsTypeCommon, &encVideoParams);
        if (status != 0)
        {
            printf("encodeSetParameters status %d\n", status);
        }

        memset(&streamFormat, 0, sizeof(streamFormat));
        streamFormat.size = sizeof(VideoConfigAVCStreamFormat);
        streamFormat.streamFormat = AVC_STREAM_FORMAT_ANNEXB;
        status = encodeSetParameters(g_sessions[index].handle, VideoConfigTypeAVCStreamFormat, &streamFormat);
        if (status != 0)
        {
            printf("encodeSetParameters status %d\n", status); 
        }

        status = encodeStart(g_sessions[index].handle);
        if (status != 0)
        {
            printf("encodeStart failed session %d status %d\n", index, status);
            return 1;
        }

    }
    for (index = 0; index < g_num_sessions; index++)
    {
        pthread_mutex_init(&(g_sessions[index].mutex), 0);
        memset(&thread, 0, sizeof(thread));
        rv = pthread_create(&thread, 0, start_routine, g_sessions + index);
        if (rv == 0)
        {
            rv = pthread_detach(thread);
            if (rv != 0)
            {
                printf("pthread_detach failed rv %d\n", rv);
                return 1;
            }
        }
        else
        {
            printf("pthread_create failed rv %d\n", rv);
            return 1;
        }
    }
    while (1)
    {
        int cr;
        for (index = 0; index < g_num_sessions; index++)
        {
            pthread_mutex_lock(&(g_sessions[index].mutex));
            cr = g_sessions[index].comp_ratio;
            printf("thread %d frames %d(fps) compression ratio %d\n", index, g_sessions[index].frames, cr);
            g_sessions[index].frames = 0;
            pthread_mutex_unlock(&(g_sessions[index].mutex));
        }
        usleep(1000 * 1000);
    }
    getchar();
    return 0;
}

