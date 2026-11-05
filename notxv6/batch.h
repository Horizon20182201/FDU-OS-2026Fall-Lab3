#ifndef LAB_BATCH_H
#define LAB_BATCH_H
/* Each worker calls one entry point once. Implement these in batch.c. */
void data_task(void);
void config_task(void);
/* Test framework callbacks: do not implement or modify them. */
void process_data(void);
void process_config(void);
#endif
