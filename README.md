This repo contains the source code for my PDP-11 emulation endeavours.

Contents
========
* a PDP-11/05 cpu (KD11B) implemented from the microcode listing, but not emulating the actual microcode. Passing ZKAAA-ZKAMA.
This is based on the code I wrote for the [Knight TV emulation](https://github.com/aap/tv11)
* a PDP-11/20 cpu (KA11) as a modified KD11B. Not implemented from the schematics (yet?). Passing ZKAAA-ZKAMA.
* a PDP-11/40 cpu (KD11A) implemented from the microcode and schematics, but not emulating the actual microcode. With KJ11 and KT11.
* the beginnings of a PDP-11/45 (KB11A) cpu. Implemented from the schematics and running the microcode. This machine is a beast so it doesn't do a whole lot at all yet.
* KL11 console
* KW11 line clock
* KE11 extended arithmetic unit implemented from the algorithm explanation in the manual. Not tested terribly well.
* simple memory

Plan
====

What I'd like is accurate emulation of the early PDP-11 cpus,
i.e. 11/05, 11/20, 11/40, 11/45, 11/70,
and at least the most important peripherals.
For the 11/45 and /70 (KB11 based) I want microcode emulation
to drive a [PiDP-11](http://obsolescence.wixsite.com/obsolescence/pidp-11) panel super duper accurately.
For the other machines microcode emulation would be interesting as well
but it's not quite as important to me.

Another goal is to have these work on an actual Unibus with
Jörg Hoppe's [Unibone](http://retrocmp.com/projects/unibone).
In fact, the 11/20 was made to work in a couple of hours
when we noticed we had implemented almost the same unibus interface
and could just hook up my code as a unibone device.

Notes
=====
The style in which I've written the KA11 and KD11B is a bit idiosyncratic.
The idea was to try and see how compact the code could be.
I have to say I like it but apologize if you don't.

To-do
=====

* implement DATIP correctly (currently done for KD11A and B)
* introduce #ifdef UNIBONE so we can maintain the code in one repo

