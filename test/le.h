#ifndef LE__H
#define LE__H

/* Public API for the LE library.
 */
int le_init();
int le_loadexpr(char *expr, char **pmsg);
double le_eval(int cookie, char **pmsg);
void le_unref(int cookie);
void le_setvar(char *name, double value);
double le_getvar(char *name);

#endif /* LE__H */
