#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <string.h>
/*function prototypes*/
static void add_entry(void* ptr, size_t size, size_t backtrace_size, void** backtrace);
static void remove_entry(void* ptr);
static void report_leaks(void);


/*Linked List that keeps track of all memory allocations*/

#define MAX_TRACE_DEPTH 16

typedef struct MemEntry {
		void* ptr;
		size_t size;
		void* backtrace_buffer[MAX_TRACE_DEPTH];
		int backtrace_size;
		struct MemEntry* next;
} MemEntry;

typedef struct AllocList {
		MemEntry* head;
		size_t len;
} AllocList;



static AllocList allocList = { 
		.head = NULL, 
		.len = 0 
};

static void* (*real_malloc)(size_t) = NULL;
static void (*real_free)(void*) = NULL;


__attribute__((constructor))
static void init(void){
		real_malloc = dlsym(RTLD_NEXT, "malloc");
		real_free = dlsym(RTLD_NEXT, "free");

		if(real_malloc == NULL || real_free == NULL){
				fprintf(stderr, "failed to hook malloc/free!\n");
				exit(1);
		}

}

static __thread void* backtrace_buffer[MAX_TRACE_DEPTH];
static __thread int inside_backtrace = 0;

void *malloc(size_t size){
		void* ptr = real_malloc(size);
		if(ptr == NULL){
				return NULL;
		}
		
		int backtrace_size;
		if(!inside_backtrace){
				inside_backtrace = 1;
				backtrace_size = backtrace(backtrace_buffer, MAX_TRACE_DEPTH);
				add_entry(ptr, size, backtrace_size, backtrace_buffer);
				inside_backtrace = 0;
		}

		return ptr;
}

void free(void* ptr){
		remove_entry(ptr);
		real_free(ptr);
}


static void add_entry(void* ptr, size_t size, size_t backtrace_size, void** backtrace_buffer) {
		if (ptr == NULL || real_malloc == NULL){
				exit(1);
		}

		MemEntry* entry = real_malloc(sizeof(MemEntry));
		if(entry == NULL){
				return;
		}
		entry->ptr = ptr;
		entry->size = size;
		memcpy(entry->backtrace_buffer, backtrace_buffer,sizeof(void*)*MAX_TRACE_DEPTH);
		entry->backtrace_size = backtrace_size;
		entry->next = allocList.head;
		allocList.head = entry;
		allocList.len++;

}

static void remove_entry(void* ptr) {
		MemEntry *curr = allocList.head, *prev = NULL;

		while (curr) {
				if (curr->ptr == ptr) {
						if (prev){
								prev->next = curr->next;
						}
						else{
								allocList.head = curr->next;
						}

				real_free(curr);
				allocList.len--;
				return;
		}
		prev = curr;
		curr = curr->next;
    }
}


static void report_leaks(void) {
		if (allocList.len == 0) {
				fprintf(stderr, "[HeapPulse] No leaks detected.\n");
		} else {
				fprintf(stderr, "[HeapPulse] %zu unfreed allocations:\n\n", allocList.len);
				size_t total_bytes = 0;
				MemEntry* curr = allocList.head;
				while (curr) {
						fprintf(stderr, "[HeapPulse]  Leak: %p (%zu bytes)", curr->ptr, curr->size);
						total_bytes += curr->size;
						char** symbols = backtrace_symbols(curr->backtrace_buffer, curr->backtrace_size);
						fprintf(stderr, " backtrack size: %i\n",curr->backtrace_size);
						for(int i = 0;i < curr->backtrace_size;i++){
								fprintf(stderr,"%s\n",symbols[i]);
						}
						curr = curr->next;
						fprintf(stderr,"---------------------------------------------------------\n\n\n");
				}
		}
}

__attribute__((destructor))
static void lib_exit(void){
		report_leaks();
}

