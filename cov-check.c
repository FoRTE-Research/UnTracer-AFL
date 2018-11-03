#define _GNU_SOURCE 
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ftw.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include "libUnTracerHashmap.h"

int devnullFD;      /* /dev/null FD for closing out STDIN/STDOUT/STDERR. */
int tcaseFD;      /* Test case FD. Utilized when piping to STDIN. */

char * queueDir;    /* User-supplied queue dir. */
char * outDir;      /* User-supplied output directory. */
char * saveDir;     /* Dir for saved inputs (currently unused) inside outDir.*/
char * plotPath;    /* Path to the exec time log inside outDir. */
char * tcasePath;   /* Path to testcase file (default: outDir/.cur_input). */
char * bblstPath;   /* Path to basic blocks list created by UnTracerDyninst (default: outDir/.bblist).*/
char * tracePath;   /* Path to testcase file (default: outDir/.cur_trace).*/
char * targetPath;    /* User-supplied path to target binary. */
char * oraclePath;    /* Path to oracle binary copy in outDir. */
char * tracerPath;    /* Path to tracer binary copy in outDir. */

int numBlocksHit;
int numBlocksTotal;
int oracleFsrvCtlFD;    /* Forkserver control pipes. */
int tracerFsrvCtlFD;
int oracleFsrvStFD;   /* Forkserver status pipes. */
int tracerFsrvStFD; 
int oracleFsrvPID;    /* Target forkserver PIDs. */
int tracerFsrvPID;
int oracleChildPID;   /* Target child PIDs. */
int tracerChildPID;

FILE * plotFile;    /* Exec time log file. */
FILE * dumpFile;    /* Input dump file. */
FILE * sizesFile;   /* Input sizes file. */

char ** oracleArgv;   /* Target arguments. */
char ** tracerArgv;

int childTimedOut;    /* Used for coordinating timeouts. */
int timeout;      /* Exec timeout duration, in ms. */
int timeoutGiven;   /* Flag to test if user supplied timeout. */

long targetSize;    /* Size of target binary. */


map_t bbHashMap;    /* Basic block hashmap data struct. */  
typedef struct hashmap_entry_struct
{
  char entryKey[256];
} hashmap_entry;
hashmap_entry* bbHmEntry;

#define ORACLE_FORKSRV_FD 198     /* Hardcoded forkserver FD's. Utilized in IPC. */
#define TRACER_FORKSRV_FD 200
#define DEFAULT_TIMEOUT 1000    /* Hardcoded default timeout, in ms */

int is_dir(char * path) {

  /* Check if a given pathname is a directory. */

  struct stat st;
  if (stat(path, &st) != 0)
     return 0;
  return S_ISDIR(st.st_mode);
}


void execute(char * tmp[], char * pidName, int printOutput){

  /* Helper function for running execve() with error checking
   * and output-printing toggling. */

  int pidFork, status;

  pidFork = fork();
  if (pidFork < 0 ){
    fprintf(stderr, "%s: %s\n", pidName, strerror(errno));
    exit(EXIT_FAILURE);
  }
  if (pidFork == 0){
    if (!printOutput){
      dup2(devnullFD, 1); 
      dup2(devnullFD, 2); 
    }
    if (execvp(tmp[0], tmp) < 0)
      fprintf(stderr, "%s: %s\n", pidName, strerror(errno));
    exit(0);
  }
  if (waitpid(pidFork, &status, 0) <= 0){
    fprintf(stderr, "%s: %s\n", pidName, strerror(errno));
    exit(EXIT_FAILURE);
  }

  return;
}


void remove_dir_recursively(char * dirToRemove){

  /* Recursively remove the directory given by path dirToRemove. */

  struct dirent* d_ent;
  char * tmpDirEntry;
  DIR* tmpDir;

  tmpDir = opendir(dirToRemove);
  if (!tmpDir) return;

  while ((d_ent = readdir(tmpDir))) {

    /* Skip recursing on ".." or ".". */
    if (!strcmp(d_ent->d_name, "..") || !strcmp(d_ent->d_name, ".")) continue;

    tmpDirEntry = malloc(strlen(dirToRemove)+2+strlen(d_ent->d_name));
    sprintf(tmpDirEntry, "%s/%s", dirToRemove, d_ent->d_name);

    /* Recurse on subdirectories. */
    if (is_dir(tmpDirEntry))
      remove_dir_recursively(tmpDirEntry);

    unlink(tmpDirEntry);
  }
  closedir(tmpDir);

  if (rmdir(dirToRemove) < 0) {
    perror("rmdir() outDir");
    exit(EXIT_FAILURE);
  }
  return;
}


void setup_out_dir(){

  /* Set up output directory. If it exists, 
   * delete its contents recursively, then create. */

  struct stat st = {0};
  
  if (stat(outDir, &st) == -1) {  
    if (mkdir(outDir, 0777) < 0){
      perror("mkdir() outDir");
      exit(EXIT_FAILURE);
    }
  }
  else {
    remove_dir_recursively(outDir);

    if (mkdir(outDir, 0777) < 0){
      perror("mkdir() outDir");
      exit(EXIT_FAILURE);
    }
  }
  return;
}


void handle_timeout(int sig){ 

  /* Timeout signal handler. Since timeouts can affect both 
   * tracer AND oracle binaries, we apply same handling to both. */

  if (oracleChildPID > 0) {
    childTimedOut = 1; 
    kill(oracleChildPID, SIGKILL);
  } 
  if (tracerChildPID > 0) {
    childTimedOut = 1; 
    kill(tracerChildPID, SIGKILL);
  } 
  if (oracleChildPID == -1 && oracleFsrvPID > 0) {
    childTimedOut = 1; 
    kill(oracleFsrvPID, SIGKILL);
  }
  if (tracerChildPID == -1 && tracerFsrvPID > 0) {
    childTimedOut = 1; 
    kill(tracerFsrvPID, SIGKILL);
  }
}


void copy_binary(char* srcPath, char* dstPath){

  /* Helper function to copy files. */

    struct stat st = {0};
  stat(srcPath, &st);
  char * data = malloc(st.st_size);

  FILE * srcFile = fopen(srcPath, "rb");
  FILE * dstFile = fopen(dstPath, "wb");

  fread(data, 1, st.st_size, srcFile);
  fwrite(data, 1, st.st_size, dstFile);

  fclose(srcFile);
  fclose(dstFile);

  chmod(dstPath, 0777);
  
  return;
}


void start_forkserver(int *pidFork, int *fsrvCtlFD, int *fsrvStFD, int FORKSRV_FD, char **tgtArgv) {

  /* Starts both oracle and tracer forkservers. */

  int stPipe[2], ctlPipe[2];
  int status;

  /* Open control and status pipes and verify their success. */
  if (pipe(stPipe) == -1 || pipe(ctlPipe) == -1) {
    perror("pipe() stPipe || ctlPipe");
    exit(EXIT_FAILURE);
  }

  /* Fork off (the forkserver) and check success. */
  *pidFork = fork();
  if (*pidFork < 0) {
    perror("fork() pidFork");
    exit(EXIT_FAILURE);
  }
  /* Enter child (forkserver). */
  if (*pidFork == 0) {

    /* Isolate process. Configure STDOUT/STDERR since we don't care about those. */
    setsid();
    dup2(devnullFD, 1); 
    dup2(devnullFD, 2); 

    /* If testcase path is specified, stdin is /dev/null; otherwise, testcase FD is cloned instead. */
    if (tcasePath) {
      dup2(devnullFD, 0);
    } 
    else {
      dup2(tcaseFD, 0);
      close(tcaseFD);
    }

    /* Set up control and status pipes, close the unneeded original fds. */
    if (dup2(ctlPipe[0], FORKSRV_FD) < 0) {
      perror("dup2() ctlPipe[0]");
      exit(EXIT_FAILURE);
    }
    if (dup2(stPipe[1], FORKSRV_FD + 1) < 0) {
      perror("dup2() stPipe[1]");
      exit(EXIT_FAILURE);
    }

    close(ctlPipe[0]);
    close(ctlPipe[1]);
    close(stPipe[0]);
    close(stPipe[1]);
    close(devnullFD);

    /* This should improve performance a bit, since it 
     * stops the linker from doing extra work post-fork(). */
    if (!getenv("LD_BIND_LAZY")) 
      setenv("LD_BIND_NOW", "1", 0);

    if (execv(tgtArgv[0], tgtArgv) < 0)
      perror("execv() forkserver");
      
    exit(0);  
  }
  
  else {
    /* Close the unneeded endpoints. */
    close(ctlPipe[0]);
    close(stPipe[1]);
    
    *fsrvCtlFD = ctlPipe[1];
    *fsrvStFD = stPipe[0];

    /* Read initialization message (hopefully 4 bytes!) 
     * We use a counter to check forkserver stalling. */
    int readCounter = 0;
    while(read(*fsrvStFD, &status, 4) != 4){
      readCounter++;
      if (readCounter == 9999) {
        printf("\n\t\tForkserver not responding...\n");
        exit(EXIT_FAILURE);
      }
      continue;
    }
    return;

    /* If the above failed, try to identify what step failed. */ 
    if (waitpid(*pidFork, &status, 0) <= 0) {
      perror("waitpid() pidFork");
      exit(EXIT_FAILURE);
    }

    if (WIFSIGNALED(status)) {
      perror("Forkserver crashed!");
      exit(EXIT_FAILURE);
    }

    perror("Forkserver handshake failed!");
    exit(EXIT_FAILURE);

    return;
  }
}


int run_target(int * pidChild, int * fsrvCtlFD, int * fsrvStFD) {

  /* Runs either binary on current testcase, and report back exit code. */

  int status;
  static int prevTimedOut = 0;
  static struct itimerval it;
  childTimedOut = 0;

  /* Forkserver is up, so tell it to have at it (control pipe), then read back PID. 
   * A message length of 4 indicates successful communication with the forkserver. */
  if (write(*fsrvCtlFD, &prevTimedOut, 4) != 4) {
    perror("write() fsrvCtlFd");
    exit(EXIT_FAILURE);
  }

  /* Read status message and check message length. */
  if (read(*fsrvStFD, pidChild, 4) != 4) {
    perror("read() fsrvStFd #1");
    exit(EXIT_FAILURE);
  }

  /* Verify child PID isn't corrupt or active. */
  if (*pidChild <= 0) {
    perror("pidChild <= 0");
    exit(EXIT_FAILURE);
  }

  /* Configure timer. */
  it.it_value.tv_sec = (timeout / 1000);
  it.it_value.tv_usec = (timeout % 1000) * 1000;
  setitimer(ITIMER_REAL, &it, NULL);

  /* Read and check message length. */
  if (read(*fsrvStFD, &status, 4) != 4) {
    perror("read() fsrvStFD #2");
    exit(EXIT_FAILURE);
  }

  if (!WIFSTOPPED(status)) 
    *pidChild = 0;

  /* Deactivate timer. */
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &it, NULL);
  prevTimedOut = childTimedOut;

  /* Report back the exit signal code. */ 
  if (WIFSIGNALED(status))
    return WTERMSIG(status); 

  else 
    return 0;
}


void usage(char ** argv) {

  /* Displays usage hints. */

  printf("\n\nUsage:\t%s [ parameters ] -- /path/to/fuzzed_app [ ... ]\n\n"

  "Options:\n"

  " -q dir      - input queue path\n"
  " -o dir      - output working directory\n"
  " -f file     - output results file\n"  
  " -t int      - (optional) exec timeout (ms)\n\n",
  argv[0]);
  exit(1);
}


void setup(int argc, char ** argv){

  /* Sets up all program args, file descriptors, and output directory. */
  
  struct stat st = {0};
  int opt;
  timeoutGiven = 0;

  /* Collect args and set variables. */
  while ((opt = getopt(argc, argv, "+q:f:o:c:t:m:")) != -1) {
    
    switch(opt) {
      case 'q':
        if (!queueDir){
          queueDir = optarg;
        }
        break;
      case 'o':
        if (!outDir){
          outDir = optarg;
          /* If last character of outDir is "/", strip it. */
          if (outDir[strlen(outDir)-1] == '/')
            outDir[strlen(outDir)-1]= '\0'; 
          }
        break;    
      case 'f':
        if (!plotPath){
          plotPath = optarg;
        }
        break;
      case 't':
        if (!timeout){
          timeout = atoi(optarg);
          timeoutGiven = 1;
        }
        break;
    }
  }

  /* Verify existence of arguments. */
  if (!queueDir){
    printf("Missing queueDir!");
    usage(argv);
    exit(EXIT_FAILURE);
  }
  if (!outDir) {
    printf("Missing outDir!");  
    usage(argv);
    exit(EXIT_FAILURE);
  }
  if (!plotPath) {
    printf("Missing plotPath!");  
    usage(argv);
    exit(EXIT_FAILURE);
  }

  /* TODO: This might be a good place for some kind of check_inst_mode()
   * function to verity that the target binary matches the instrumentation
   * mode selected by the user. */

  /* Verify file paths. */
  if (access(queueDir, F_OK ) == -1) {
    perror("access() queueDir");
    exit(EXIT_FAILURE);
  }

  /* Set up timeout (SIGALARM) handling. 
   * If no timeout provided, use default. */
  signal(SIGALRM, handle_timeout);
  if (timeoutGiven == 0)
    timeout = DEFAULT_TIMEOUT; 

  /* Set up basic block list, trace, and testcase paths. */
  bblstPath = malloc(strlen(outDir)+2+strlen(".bblist"));
  sprintf(bblstPath, "%s/.bblist", outDir);

  tracePath = malloc(strlen(outDir)+2+strlen(".cur_trace"));
  sprintf(tracePath, "%s/.cur_trace", outDir);

  tcasePath = malloc(strlen(outDir)+strlen("/.cur_input")+1);
  sprintf(tcasePath, "%s/.cur_input", outDir);

  /* Set up target binary path and args. Used later. */
  char ** targetArgv = argv + optind;
  targetPath = targetArgv[0];
  oracleArgv = malloc((argc-optind+1) * sizeof(targetArgv));
  tracerArgv = malloc((argc-optind+1) * sizeof(targetArgv));

  /* Set up relevant paths. */
  oraclePath = malloc(strlen(outDir)+2+strlen(basename(targetPath))+strlen(".oracle"));
  sprintf(oraclePath, "%s/%s.oracle", outDir, basename(targetPath));
  
  tracerPath = malloc(strlen(outDir)+2+strlen(basename(targetPath))+strlen(".tracer"));
  sprintf(tracerPath, "%s/%s.tracer", outDir, basename(targetPath));

  /* If present, replace "@@" with tcasePath. */
  /* TODO - tcaseFD STDIN configuration. */
  for(int i = 0; i<argc-optind; i++)
  {
    if (strcmp(targetArgv[i], "@@") == 0)
      targetArgv[i] = tcasePath;
    memcpy(&oracleArgv[i], &targetArgv[i], sizeof(targetArgv[0]));
    memcpy(&tracerArgv[i], &targetArgv[i], sizeof(targetArgv[0]));  
  }

  /* Set argument target copy paths and NULL terminators. */
  oracleArgv[0] = oraclePath;
  tracerArgv[0] = tracerPath;
  oracleArgv[argc-optind] = NULL;
  tracerArgv[argc-optind] = NULL;

  /* Set up target size. */
  stat(targetPath, &st);
  targetSize = st.st_size;

  /* Set up output directory. */
  setup_out_dir();
  
  /* Set up file descriptors for /dev/null and testcase. */
  devnullFD = open("/dev/null", O_RDWR);
  if (devnullFD < 0) {
    perror("open() /dev/null");
    exit(EXIT_FAILURE);
  }

  tcaseFD = open(tcasePath, O_RDWR | O_APPEND | O_CREAT, 0600);
  if (tcaseFD < 0) {
    perror("open() testcase");
    exit(EXIT_FAILURE);
  }

  return;
}

void modify_oracle(){

  /* Modifies the start of every basic block in the oracle binary copy. 
   * Only basic blocks recorded in the hashmap are considered; 
   * a previous step pruned basic blocks occurring before main() to 
   * prevent forkserver failure. */

  char line[256];
  int addr;
  char flag[1] = {0xCC};
  int offset = 0;

  /* Open both target oracle and bb list files. */
  FILE * bblstFile = fopen(bblstPath, "r");
  FILE * tgtFsrvFile = fopen(oraclePath, "r+");

  /* Iterate through bb list; insert the "CC" interrupt for every corresponding bb address in the target oracle. */
  while (fgets(line, sizeof(line), bblstFile)) {

    /* We only want to alter blocks present in the hashmap. */
    if (hashmap_get(bbHashMap, line, (void**)(&bbHmEntry)) == MAP_OK){
      
      /* Skip the "0x" in the hex address, and convert to decimal. */
      addr = atoi(line) + offset;

      /* Jump to the location in oracle binary, overwrite first two bytes with 0xCC interrupt. */
      fseek(tgtFsrvFile, addr, SEEK_SET);
      fwrite(flag, 1, 1, tgtFsrvFile);

      numBlocksTotal++;
    }
  } 

  fclose(tgtFsrvFile);
  fclose(bblstFile);

  return;
}


void unmodify_oracle(){ 

  /* Resets every basic block in the oracle binary copy back to its original values. 
   * To prevent redundant unmodifying of basic blocks, we remove each unmodified basic
   * block from the global hashmap, and only consider basic blocks in the hashmap. */

  char line[256];
  int offTgt, offTgtFsrv;
  char flag[1];
  int offset = 0;

  /* Open both target, target oracle, and trace files. */
  FILE * tgtFile = fopen(targetPath, "r+");
  FILE * tgtFsrvFile = fopen(oraclePath, "r+");
  FILE * traceFile = fopen(tracePath, "r");

  /* Iterate through bb list; insert the "CC" interrupt for every corresponding bb address in the target oracle. */
  while (fgets(line, sizeof(line), traceFile)) {
    
    /* See if the basic block address exists in the hashmap. 
     * If it does, proceed to unmodify that address and remove it from the hashmap. 
     * Otherwise, continue onto the next basic block. */

    if (hashmap_get(bbHashMap, line, (void**)(&bbHmEntry)) == MAP_OK) {

      offTgt = atoi(line); 
      offTgtFsrv = offTgt + offset;

      /* Jump to the respective offs in both the target and the target oracle copy. */
      fseek(tgtFile, offTgt, SEEK_SET);
      fseek(tgtFsrvFile, offTgtFsrv, SEEK_SET);
        
      /* Read first two bytes of basic block from the target file, overwrite in the target oracle. */
      if (fread(flag, 1, 1, tgtFile) != 1){
        perror("fread() unmodify_oracle()");
        exit(EXIT_FAILURE);   
      }
      if (fwrite(flag, 1, 1, tgtFsrvFile) != 1){
        perror("fwrite() unmodify_oracle()");
        exit(EXIT_FAILURE);
      }

      /* Remove the processed address from the hashmap and verify success. */
      if (hashmap_remove(bbHashMap, line) != MAP_OK){
        perror("hashmap_remove()");
        exit(EXIT_FAILURE);
      }

      numBlocksHit++;
    }
    else
      continue;
  } 

  fclose(tgtFile);
  fclose(tgtFsrvFile);
  fclose(traceFile);

  return;
}


void stop_forkserver(int * pidFork, int * fsrvCtlFD, int * fsrvStFD){

  /* Terminates either binary's forkserver. */

  int status;

  /* Instead of telling the target binary forkserver to run another iteration,
   * tell it to die. */
  if (write(*fsrvCtlFD, &status, 2) != 2) {
    perror("write() stop command failed");
    exit(EXIT_FAILURE);
  }

  /* Close open ends of pipe. */
  close(*fsrvCtlFD);
  close(*fsrvStFD);

  /* Make sure the process dies. */
  if (waitpid(*pidFork, &status, 0) <= 0) {
    perror("waitpid() for target to stop failed");
    exit(EXIT_FAILURE);
  }
}



void setup_hashmap(){

  /* Reads a list of basic blocks from file and populates a new hashmap. */

  char line[256];
  bbHashMap = hashmap_new();
  FILE * bblstFile = fopen(bblstPath, "r");

  while (fgets(line, sizeof(line), bblstFile)) {

    bbHmEntry = malloc(sizeof(bbHmEntry));
    snprintf(bbHmEntry->entryKey, 256, "%s", line);
    
    /* If a hashmap_put() call failed, report and terminate. */
    if (hashmap_put(bbHashMap, bbHmEntry->entryKey, bbHmEntry) != MAP_OK){
      perror("hashmap_put()");
      exit(EXIT_FAILURE);
    }
  } 

  fclose(bblstFile);

  return;
}


void setup_oracle(){

  /* Set up the oracle binary. */

  printf("[*]\tSetting up oracle binary...\n");

  int targetFD;
  char * targetData = malloc(targetSize);

  targetFD = open(targetPath, O_RDONLY);
  if (targetFD < 0) {
    perror("open() targetFD");
    exit(EXIT_FAILURE);
  }

  targetData = mmap(0, targetSize, PROT_READ, MAP_PRIVATE, targetFD, 0);
  if (targetData == MAP_FAILED){
    perror("mmap() targetData");
    exit(EXIT_FAILURE); 
  }

  /* Found afl-cc-instrumented forkserver (whitebox mode). */
  if (memmem(targetData, targetSize, "__afl_maybe_log", strlen("__afl_maybe_log") + 1))
    copy_binary(targetPath, oraclePath);    
  
  else {
    printf("\n\t\tForkserver not found in target %s!\n", basename(targetPath));
    exit(EXIT_FAILURE);
  }

  return;
}


void setup_tracer(){

  /* Set up the tracer binary. */

  printf("[*]\tSetting up tracer binary...\n");

  copy_binary(targetPath, tracerPath);

  char nop[1] = {0x90};
  char * data = malloc(targetSize);
  long int offset;

  /* NOP target forkserver callback since we don't want conlifcts with Dyninst's forkserver. */
  /* Find the starting offset - where the three consecutive nops lie. */
  FILE * tracerFile = fopen(tracerPath, "r+");
  fread(data, 1, targetSize, tracerFile);
  offset = (char *) memmem(data, targetSize, "\x90\x90\x90", strlen("\x90\x90\x90"+1)) - data;

  /* Replace the first 5 bytes after the nop. */
  for (int i=0; i<9; i++){
    fseek(tracerFile, offset+3+i, SEEK_SET);
    fwrite(nop, 1, 1, tracerFile);
  }
  fclose(tracerFile); 

  char * dyninstArgs[] = {"UnTracerDyninst", tracerPath, "-O", tracerPath, "-M", "2", "-F", "-T", tracePath, "-H", "-I", bblstPath, NULL};
  
  execute(dyninstArgs, "UnTracerDyninst", 0);

  return;
}

int descending_alphasort(const struct dirent **a, const struct dirent **b)
{
  /* Taken from here: 
   * https://stackoverflow.com/questions/37974593/sort-files-in-descending-alphabetical-order */
  return alphasort(b, a);
}

long get_file_timestamp(char *filePath)
{
  /* Taken from here: 
   * https://stackoverflow.com/questions/10446526/get-last-modified-time-of-file-in-linux */
  char * end;
  char *date = malloc (sizeof (char) * 256);
  struct stat attrib;
  stat(filePath, &attrib);
  strftime(date, 256, "%s", gmtime(&(attrib.st_ctime)));
  long dateLong = strtol(date, &end, 10);
  return dateLong;
}

long get_starting_timestamp(int size, struct dirent **contents){
    /* First pass to identify non-id queue contents (e.g., ".state", "../", etc.). */

    int sizeCopy = size;
    int numToExclude = 0;

    while (sizeCopy--)
      if (strstr(contents[sizeCopy]->d_name, "id:") == NULL)
        numToExclude++;

    char * inputPath = malloc(strlen(queueDir)+2+strlen(contents[size-numToExclude-1]->d_name));
    sprintf(inputPath, "%s/%s", queueDir, contents[size-numToExclude-1]->d_name);

    return get_file_timestamp(inputPath);
}

int main(int argc, char ** argv){

  /* Print banner. */
  printf("\n[ UnTracer-AFL cov-check utility | FoRTE-Research @ Virginia Tech | based on afl-fuzz by <lcamtuf@google.com>]\n\n");

  int isInteresting;
  int countInteresting = 0;
  int countQueued = 0;
  int interruptSig = 5;
  struct dirent **queueNames;
  int queueNum;
  char * inputPath;

  /* Set up fd's, globals, file objects. */
  setup(argc, argv);

  printf("[*]\tPreparing binaries...\n");
  setup_oracle();
  setup_tracer();
  printf("[*]\tFinished preparing binaries!\n");

  printf("[*]\tSetting up hashmap...\n");
  setup_hashmap();
  printf("[*]\tFinished hashmap setup!\n");

  printf("[*]\tModifying oracle binary...\n");
  modify_oracle();
  printf("[*]\tFinished modifying oracle!\n");

  /* Start up trace forkserver and perform dry run to unmodify seed coverage set,
   * Then, get the oracle forkserver up and running. */
  start_forkserver(&tracerFsrvPID, &tracerFsrvCtlFD, &tracerFsrvStFD, TRACER_FORKSRV_FD, tracerArgv);
  //perform_dry_run(); // commented out, for now
  start_forkserver(&oracleFsrvPID, &oracleFsrvCtlFD, &oracleFsrvStFD, ORACLE_FORKSRV_FD, oracleArgv);

  plotFile = fopen(plotPath, "w+");
  if(plotFile == NULL) {
    printf("Error opening results fileQ: %s\n", plotPath);
    exit(EXIT_FAILURE);
  }
 
  /* Helpful info. */
  printf("[*]\tStarting evaluation...\n");
  printf(timeoutGiven ? \
    "[*]\tTimeout = %i ms\n" : \
    "[*]\tTimeout not specified; selecting default (%i ms)\n", timeout);

  printf("[*]\tStarting evaluation...\n");

  queueNum = scandir(queueDir, &queueNames, NULL, descending_alphasort);

  if (queueNum < 0){
    perror("scandir");
    exit(EXIT_FAILURE);
  }
  
  else {

    unsigned long startTime = get_starting_timestamp(queueNum, queueNames);

    fprintf(plotFile, "tcase_name, is_interesting, num_interesting, num_blocks_total, num_blocks_hit, time_start, time_created\n");

    while (queueNum--) {

      if (strstr(queueNames[queueNum]->d_name, "+cov") != NULL) {

        isInteresting = 0;
        inputPath = malloc(strlen(queueDir)+2+strlen(queueNames[queueNum]->d_name));
        sprintf(inputPath, "%s/%s", queueDir, queueNames[queueNum]->d_name);
        
        copy_binary(inputPath, tcasePath);

        if (run_target(&oracleChildPID, &oracleFsrvCtlFD, &oracleFsrvStFD) == interruptSig){

          isInteresting = 1;
          stop_forkserver(&oracleFsrvPID, &oracleFsrvCtlFD, &oracleFsrvStFD);
          run_target(&tracerChildPID, &tracerFsrvCtlFD, &tracerFsrvStFD);
          unmodify_oracle();
          start_forkserver(&oracleFsrvPID, &oracleFsrvCtlFD, &oracleFsrvStFD, ORACLE_FORKSRV_FD, oracleArgv);

          countInteresting++;

        }

        countQueued++;

        fprintf(plotFile, "%s, %i, %i, %i, %i, %lu, %lu\n", queueNames[queueNum]->d_name, isInteresting, countInteresting, numBlocksTotal, numBlocksHit, startTime, get_file_timestamp(inputPath));
        
        free(queueNames[queueNum]);

      } 
    }
    free(queueNames);
  }

  printf("[*]\tFinished!\n");
  printf("[*]\tNum. queued with \"+cov\"\t\t= %i\n[*]\tNum. hitting new blocks\t\t= %i\n", countQueued, countInteresting);
  printf("[*]\tPct. hitting new blocks\t\t= %.3f%%\n", (float) 100*countInteresting/countQueued);
  printf("[*]\tPct. of all blocks hit\t\t= %.3f%%\n", (float) 100*numBlocksHit/numBlocksTotal);
  /* Wrap things up and terminate. */
  stop_forkserver(&oracleFsrvPID, &oracleFsrvCtlFD, &oracleFsrvStFD);
  stop_forkserver(&tracerFsrvPID, &tracerFsrvCtlFD, &tracerFsrvStFD);
  fclose(plotFile);
  hashmap_free(bbHashMap);     

  return 0;
}


