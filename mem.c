#include "11.h"

int
dati_mem(Bus *bus, void *dev)
{
	Memory *mem = dev;
	uint32 waddr = bus->addr>>1;
	if(waddr >= mem->start && waddr < mem->end){
		bus->data = mem->mem[waddr-mem->start];
		return 0;
	}
	return 1;
}

int
dato_mem(Bus *bus, void *dev)
{
	Memory *mem = dev;
	uint32 waddr = bus->addr>>1;
	if(waddr >= mem->start && waddr < mem->end){
		mem->mem[waddr-mem->start] = bus->data;
		return 0;
	}
	return 1;
}

int
datob_mem(Bus *bus, void *dev)
{
	Memory *mem = dev;
	uint32 waddr = bus->addr>>1;
	if(waddr >= mem->start && waddr < mem->end){
		mem->mem[waddr-mem->start] &= (bus->addr&1) ? 0377 : ~0377;
		mem->mem[waddr-mem->start] |= bus->data & ((bus->addr&1) ? ~0377 : 0377);
		return 0;
	}
	return 1;
}
