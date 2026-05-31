/* pcuserio.h — stub for headless/Linux PC_WRAPPER build
 * PCUserIO_IsConnected() always returns FALSE — no commissioning terminal. */
#ifndef PCUSERIO_H
#define PCUSERIO_H

static inline int PCUserIO_IsConnected(void) { return 0; }

#endif /* PCUSERIO_H */
