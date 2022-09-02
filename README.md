# Copy_system_in_xv6OS
This was a university project we had as homework. Original project text is in the "OS Domaci1" file.

I added a copy paste functionality to the xv6 OS

The xv6 OS can be run on a linux machine, to do so
put these files into a folder, name it however you like, 
and then go to that directory through the terminal 
(exp. folder named xv6 would be accessed as "cd xv6"
if it is in the main "." directory).
Once youre inside the folder simply write "make qemu"
to run xv6 and it will bootup.

To use the copy functionality press SHIFT+ALT+C to enter the
copy mode. In this mode you can move around freely with 
'w', 'a', 's', 'd' keys. To start your copy selection press 'q'
while in copy mode, and move around to select the text you want to
copy (keep in mind that the maximum number of characters you can 
copy is about 128 and no more, anything larger will be cut off).
The text selected will be highlighted with a white background and
black letters. When you are ready to end your selection press 'q'
(again keep in mind this only works if your in copy mode and you
already started your selection with 'q'). And now you're back in 
normal copy mode, you can move around and make a new selection
which will overwrite the old one, or you can press SHIFT+ALT+C
to end the copy mode which will return the cursor to its normal
position. If you're ready to paste what you copied simply press
SHIFT+ALT+P and the text will be copied to the terminal.

Cheers!
