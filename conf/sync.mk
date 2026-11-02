# Host-side pthread targets; independent of xv6 CC/CFLAGS and LAB=lock.
HOSTCC ?= gcc
HOSTCFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -D_POSIX_C_SOURCE=200809L

queue_test: notxv6/queue.c notxv6/queue.h notxv6/tests/queue_test.c notxv6/tests/common.h
	$(HOSTCC) $(HOSTCFLAGS) -Inotxv6 -Inotxv6/tests notxv6/queue.c notxv6/tests/queue_test.c -o $@

batch_test: notxv6/batch.c notxv6/batch.h notxv6/tests/batch_test.c notxv6/tests/common.h
	$(HOSTCC) $(HOSTCFLAGS) -Inotxv6 -Inotxv6/tests notxv6/batch.c notxv6/tests/batch_test.c -o $@

grade-sync:
	python3 ./grade-lab-sync $(SYNC_GRADEFLAGS)

grade-queue:
	python3 ./grade-lab-sync queue $(SYNC_GRADEFLAGS)

grade-batch:
	python3 ./grade-lab-sync batch $(SYNC_GRADEFLAGS)

clean-sync:
	rm -f queue_test batch_test

.PHONY: grade-sync grade-queue grade-batch clean-sync
