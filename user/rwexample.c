#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  const int workers = 4;

  if(rwexample(-1) < 0){
    printf("rwexample: initialization failed\n");
    exit(1);
  }

  for(int role = 0; role < workers; role++){
    int pid = fork();
    if(pid < 0){
      printf("rwexample: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      int result = rwexample(role);
      exit(result == 0 ? 0 : 1);
    }
  }

  int passed = 0;
  for(int i = 0; i < workers; i++){
    int status;
    if(wait(&status) > 0 && status == 0)
      passed++;
  }

  if(passed == workers){
    printf("rwexample: PASS (all snapshots were consistent)\n");
    exit(0);
  }

  printf("rwexample: FAIL (%d/%d workers passed)\n", passed, workers);
  exit(1);
}
