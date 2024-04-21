This is a tool that can convert simple XB code to GPL.

It's very restricted but can produce working code, so
you can protype in XB and then compile to GPL for use
in cartridges.

xb2gpl <input text file> <base address in hex> <output GROM file>

The input text file is your XB program. It is not recommended
to hand-code this, rather, export it from a running XB session
(using Classic99 is the easiest, you can simply LIST to either
a Windows text file, or LIST "CLIP" and paste into a notepad).
The code is not guaranteed to be tolerant of whitespace, so 
you will have the best chance with that.

Base address in Hex is where the GPL is expected to run. This
does not create a cartridge, just a raw binary GPL file.

If you want to run this in Classic99, and are not sure how to
make a header, you can use this Usercart definition along with
the included header file.

[Usercart0]
Name="Converted XB"
rom0=G|6000|0020|mods\classic99Hdr.bin
rom1=G|6020|1fe0|mods\myprogram.bin

Copy the classic99Hdr.bin to the mods folder, and save your output
program there too. Use a start address of 6020.

Step-by-Step to use with Classic99:

1 - Before you start Classic99, add the Usercart definition
above to Classic99.ini. If you already have Usercarts in there,
just make it the next numbered one instead of 0.

2 - Copy classic99Hdr.bin into your Classic99 "mods" folder.

(you only have to do the above two steps the first time)

3 - Start Classic99

4 - Select Extended BASIC (Cartridge->Apps->Extended BASIC)

5 - Press '2' twice to get into Extended BASIC

6 - Copy and paste, or type, the following simple program:

10 CALL CLEAR
20 CALL CHAR(42, "7E81A581A599817E")
30 DISPLAY AT(12,1):"* HELLO WORLD! *"
40 GOTO 40

(if you run it, use FCTN-4 to stop)

7 - Open a copy of Notepad (Windows - Start->Run, type Notepad)

8 - In Classic99, type the following just so, quotes and all: LIST "CLIP"

9 - When it is done, switch to Notepad and select Edit->Paste. (If you get nothing,
do step 8 again and don't allow any programs to interrupt you before step 9!

10 - save the file with File->Save. Save it to the same folder that you unpacked
this converter to, pick an easy to find folder. Call it "myprogram.txt".

11 - Open a command prompt - in Windows 2000 and later, use Start->Run, and type
"cmd". For Windows 95/98/ME, type "command".

12 - change to the folder you unzipped this program in with the "cd" command. For
instance, on my computer I type "cd \work\ti\xb2gpl" and press enter. The prompt
will change to the path if you are successful.

13 - Convert the file with "xb2gpl myprogram.txt 6020 myprogram.bin". You should get
something like this:

C:\WORK\TI\xb2gpl\release>xb2gpl.exe \myprogram.txt 6020 \classic99\mods\myprogram.bin
xb2gpl <input text file> <grom base address in hex> <output text file>
*
  Invalid line number or no keyword?
*10 CALL CLEAR
  6020: ALL >20
* 07,20,

*20 CALL CHAR(42,"7F81A581A599817F")
*30 DISPLAY AT(12,1):"* HELLO WORLD! *"
  6022: MOVE >0008 TO VDP@>0950 FROM GROM@>602C
  6029: B GROM@>6034
  602C: BYTE >7F,>81,>A5,>81,>A5,>99,>81,>7F,
* 31,00,08,A9,50,60,2C,05,
* 60,34,7F,81,A5,81,A5,99,
* 81,7F,

*30 DISPLAY AT(12,1):"* HELLO WORLD! *"
  6034: FMT
  6035:   COL 3
  6037:   ROW 12
  6039:   HTEX 16 "* HELLO WORLD! *"
*40 GOTO 40
  604A: ENDFMT
* 08,FF,02,FE,0B,0F,2A,20,
* 48,45,4C,4C,4F,20,57,4F,
* 52,4C,44,21,20,2A,FB,

*40 GOTO 40
  604B: B G@L40
* 05,00,00,

* Absolute fixups:
*  >002C (>604C): G@L40 = G@>604B

* Relative fixups:

14 - Copy the myprogram.bin into your Classic99/mods folder (if necessary)

15 - Reset Classic99 by selecting Cartridge->User->Converted XB

16 - Select 'COMPILED XB' from the program menu.

17 - Cheer because it worked! (Fctn-= to quit)

------------------
Supported Commands
------------------

Not very much - and note that errors are loosely reported, but you have to search for them.
It's a hacky little tool and may or may not be useful to anyone. I may extend it if people
consider it useful. Syntax is strict and spacing is generally important.

Strings are not supported, and all variables are just 8 bits unsigned (0-255). Variables are
assigned starting at >8300 and there is no test for using too much memory.

The resulting code may not exceed a single GROM in size (8k).

REM -	ignored, EXCEPT in the special case where you can use REM to embed actual
		GPL code in your XB, by using "REM GPL". These are documented next.
		
REM GPL AORG - specify a new absolute address to continue code at. This overrides the command line.
		This should only be used before any code is generated, as a filler gap is not produced in the
		output file.
		REM GPL AORG >6200
		- changes the new GROM address to >6200
		
REM GPL BSS	- This tool allocates variables in scratchpad starting at >8300. This lets you reserve
		a block. It's only useful as the first command before any other variables, otherwise you
		don't know what address the space is at!
		REM GPL BSS 6		
		- reserves 6 bytes of scratchpad
		
REM GPL MOVE - Inserts a GPL MOVE command. All modes except GROM indexed are supported. The
		syntax and order must be exactly as shown here. CPU addresses are specified as @,
		GROM as G@, VDP as V@, and VDP Registers as R@. Values may be hex or decimal, hex is
		indicated by '>'.
		REM GPL MOVE >0010 TO V@>0300 FROM G@>6100
		-stores a GPL move command that moves 16 bytes from GROM >6100 to VDP >0300
		
REM GPL XML - Inserts a GPL XML command. The most common use is to CALL LOAD an address to
        -32000 (>8300) so you can use XML >F0 to call assembly code.
        REM GPL XML >F0
        - Stores a GPL XML command with argument >F0
		
DISPLAY AT - Puts strings on the screen. Only this one strict syntax is supported.
		DISPLAY at will be combined with other ajacent DISPLAY, HCHAR, or VCHAR commands
		into a single FMT statement. Variables are not supported.
		DISPLAY AT(10,12):"My string"
		-displays "My string" at position 10,12.

GOSUB -	Branches to a subroutine.
		GOSUB 2100
		- branches to a subroutine at line 2100
		
IF -	Performs a test and branch. Only the strict format of comparing two values and then
		jumping to a line number is supported, no multiple statements, no ELSE. The left
		side of the comparison must be a variable. The right side may be a variable or a
		number. The comparisons =, >, < and <> are supported.
		IF A=1 THEN 280
		- branches to line 280 if A has the value 1
		
GOTO -	Branches to a new line number unconditionally.
		GOTO 150
		- branches to line 150

STOP -	Ends execution and returns to the master title page
END -	END
		- Program ends and console restarts
		
RETURN - Returns from a subroutine invoked with GOSUB
		RETURN
		-Returns to the last GOSUB statement
		
LET -	Performs assignment to a variable. The word LET is optional. The syntax is
		<variable> = <expression>, with a limited number of expressions available.
		Expressions are evaluated left-to-right without order of operations, and 
		parentheses are not supported, so you may need to break an expression up to
		make it run correctly in XB. Only addition and subtractio are supported,
		though you may mix constants and variables. Also, you can invert a single
		value with NOT <variable> -- NOT may not be used in an mathematical expression.
		A=K-48
		-Assigns the variable 'A' the value of the variable 'K' minus 48
		
CALL INIT - does nothing at all, provided since XB needs it sometimes
		CALL INIT
		-nothing is emitted		
		
CALL CHARSET - loads the uppercase character set into VDP RAM. (You have to do lowercase
		manually with a MOVE if you want that).
		CALL CHARSET
		-Uppercase character set is loaded
		
CALL CHAR - defines a character (or up to 4 characters at once). Multiple CALL CHAR
		statements, if consecutive and defining consecutive characters, will be
		combined into a single MOVE statement for better performance.
		CALL CHAR(97,"0101010101010101")
		-Redfines lowercase 'a' (ASCII 97) into a vertical line.

CALL KEY - scans the keyboard with the specified mode, returning the key value and
		status. The status variable can only be tested immediately after the CALL KEY,
		otherwise it is lost. The keyboard variable will be assigned explicitly to
		address >8375 and thus always be associated with the key value. If no key is
		pressed, the key value is 255. The status value is 0 (no key pressed) or not
		0 (key is pressed), the XB value for held key is not supported. Scanning in
		modes 1 or 2 also reads the joysticks, and you can get those values with
		CALL PEEK, see the Editor/Assembler manual for 'SCAN'.
		CALL KEY(0,K,S)
		-Scans the keyboard in mode 0, returning the key value as 'K'. 'S' may be tested
		as pressed or not only as the very next line, otherwise it is lost. A variable
		is not defined for 'S'.

CALL HCHAR - repeats character horizontally across the screen, with wrap. You may pass
		three arguments for Row, Column, and Character respectively, or add a fourth 
		for repeat count. Variables are not supported. HCHAR will combine with adjacent
		DISPLAY and VCHAR statements in a single FMT object.
		CALL HCHAR(12,1,42,32)
		-Draws a row of 32 asterisks (ASCII 42) across the middle of the screen
		
CALL VCHAR - repeats character vertically across the screen, with wrap. You may pass
		three arguments for Row, Column, and Character respectively, or add a fourth 
		for repeat count. Variables are not supported. VCHAR will combine with adjacent
		DISPLAY and HCHAR statements in a single FMT object.
		CALL VCHAR(1,16,42,24)
		-Draws a column of 24 asterisks (ASCII 42) down the middle of the screen

CALL LOAD - 'poke's values into memory, you may specify more than one value to load and
		you may specify variables or constants for the values (the address must be a
		constant).
		CALL LOAD(-32000,A,15)
		-Stores the value in variable 'A' at >8300, and the value 15 at >8301

CALL PEEK - 'peek's values from memory, you may specify more than one value to read.
		The address must be a constant, the read values must all be variables.
		CALL PEEK(-32000,A,B)
		-Stores the value from >8300 in A, and the value from >8301 in B

CALL CLEAR - clears the screen with spaces (ASCII 32)
		CALL CLEAR
		- the screen is cleared.

