//prototypes
void * dm_malloc(size_t);
void * dm_realloc(void *, size_t);
void dm_free(void *);
void * dm_calloc(size_t, size_t);

//initializers
void carve(int); //divide up avaliable memory
void initialize_group(int);

typedef union{
	struct{
		size_t blocksize; // which free list to insert freed items into
        struct header * next;   // ONLY USED IF NEED PPR-local allocated objects (commit, PPR-local GC)
	} allocated;
	struct{
        //doubly linked free list for partioning
		struct header * next;
		struct header * prev;
	} free;
} header;

//data accessors for merge time
void get_lists(header* freed, header* allocated); //give data
void update_internal_lists(header * freed, header * allocated); //update my data
