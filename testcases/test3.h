
extern void Tconsole(char *fmt, ...);
extern void PrintStats(void);

#define verify(expr) {							\
    if (!(expr)) {							\
	printf("%s:%d: verify failed `%s'\n", __FILE__, __LINE__, 	\
	    #expr);							\
	abort();							\
    }									\
}
#define PAGE_ADDR(page) ((char *) (vmRegion + ((page) * MMU_PageSize())))

