# What This Is

This is a bare bones 32X project using Chilly Willy's 32X Toolkit.

This project is meant as a base example for working with C on the 32X and MD side. It is only a basic shell to get up and running. 

It is based off of the work of Victor Luchits with his Doom 32X Ressurection project
https://github.com/viciious/d32xr

# Why It Exists

I'm not an assembly language programmer and prefer working in C. The Doom32XR project was the only 32X project I could find that used C on the MD side that actually worked, so I ripped out everything related to Doom and kept the 32X specific parts.

# Requirements

Chilly Willy's 32X toolchain
 	
	sega-toolchain-12.1.7z
		
	Direct link
	
	https://drive.google.com/file/d/1C-mTeyLMPlQ6gX7DMarPlfEayTBH312t/view
		
	Original page
		
	http://gendev.spritesmind.net/forum/viewtopic.php?t=3024

# Platforms

The toolchain is geared for Linux. I built on Windows Subsystem for Linux (WSL).
	
It may work on other platforms.

# Building

Run make from the base folder

	make -f build/Makefile_32X 
		
	make -f build/Makefile_32X TARGET=<my project name>
		
The results will go into an "out" folder at the root of the project

# License(s)

Copyright notices and licenses have been kept as-is

# Additional Links

Victor Luchitz page of 32X Information and Links
https://github.com/viciious/32XDK
