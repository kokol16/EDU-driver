
A general purpose driver was created to communicate with the fully emulated QEMU's edu device.
The edu model emulates a device with storage, a couple of registers, DMA support and interrupts.
It also supports the calculation of a factorial by setting a register with which factorial we would like to find.


Firstly, we have to enable edu in QEMU by the cmd switch -device edu.
Then our driver has to first register itself as a driver for the edu device, so for 1234:11e8.

We then allocate the register memory mapped io memory and we can use the device's registers. In order to calculate 
a factorial, we jus set the factorial register to the user buffer, so whatever the user wants to calculate, and then
read that buffer once the factorial is calculated (spinning on the ready status).

To implement DMA transfer, we have to correctly set up the DMA registers (0x80 +) in order to make a DMA transfer between
our kernelspace and the device. To make a DMA transfer, we need a shared memory page which we allocate with dma_alloc_coherent,
that will give us a link between our virtual addresses and DMA physical addresses.

To move on to interrupts, we would have to enable interrupts by doing request_interrupts and specify an interrupt handler where
we would have to acknowledge the interrupt.

