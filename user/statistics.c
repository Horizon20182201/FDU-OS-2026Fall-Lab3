#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
statistics(void *buf, int sz)
{
  int fd, i, n;
  
  fd = open("statistics", O_RDONLY);
  if(fd < 0) {
      fprintf(2, "stats: open failed\n");
      exit(1);
  }
  for (i = 0; i < sz; ) {
    if ((n = read(fd, buf+i, sz-i)) < 0) {
      break;
    }
    i += n;
  }
  close(fd);
  return i;
}

static int
starts_with(const char *s, const char *prefix)
{
  while(*prefix)
    if(*s++ != *prefix++)
      return 0;
  return 1;
}

// Sum the existing per-lock counters. Stop before the top-five section,
// which can repeat locks already included in the kmem/bcache section.
void
lockstats_parse(const char *buf, struct lockstats *snapshot)
{
  memset(snapshot, 0, sizeof(*snapshot));
  for(const char *line = buf; *line; ){
    if(starts_with(line, "--- top 5"))
      break;
    char *end = strchr(line, '\n');
    if(end == 0)
      break;
    int group = -1;
    if(starts_with(line, "lock: kmem"))
      group = 0;
    else if(starts_with(line, "lock: bcache"))
      group = 1;
    if(group >= 0){
      char *tas = strchr(line, '#');
      char *acq = tas ? strchr(tas + 1, '#') : 0;
      if(tas == 0 || acq == 0 || acq >= end ||
         !starts_with(tas, "#test-and-set ") ||
         !starts_with(acq, "#acquire() ")){
        fprintf(2, "lockstats: invalid counters\n");
        exit(1);
      }
      snapshot->nts[group] += atoi(tas + strlen("#test-and-set "));
      snapshot->acquire[group] += atoi(acq + strlen("#acquire() "));
    }
    line = end + 1;
  }
}

// Print only after the end snapshot, keeping output outside the measurement.
void
lockstats_diff(const char *test, const struct lockstats *before,
               const struct lockstats *after)
{
  const char *names[] = {"kmem", "bcache"};
  for(int i = 0; i < 2; i++){
    printf("%s %s: before_tas %u after_tas %u delta_tas %u "
           "before_acquire %u after_acquire %u delta_acquire %u\n",
           test, names[i], before->nts[i], after->nts[i],
           after->nts[i] - before->nts[i],
           before->acquire[i], after->acquire[i],
           after->acquire[i] - before->acquire[i]);
  }
}
