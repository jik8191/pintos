/* Declare the entry-points to the interrupt handler assembly-code fragments,
 * so that the C compiler will be happy.
 *
 * You will need lines like these:  void *(irqN_handler)(void)
 */
#ifndef HANDLER_H
#define HANDLER_H

// Assembly functions for interrupt handling
void *(irq_keyboard_handler)(void);
void *(irq_timer_handler)(void);

#endif /* HANDLER_H */

