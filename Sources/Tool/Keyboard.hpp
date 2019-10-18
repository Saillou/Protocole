#include <stdio.h>

#ifdef __linux__
	#include <termios.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/time.h>
	
#else
	#include <conio.h>
	
#endif

void changeKeybordDirection(int dir) {
#ifdef __linux__
	static struct termios oldt, newt;
	
	if(dir == 1) {
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}
	else {
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	}
#endif
}

int keyHit() {
#ifdef __linux__
	struct timeval tv;
	fd_set rdfs;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);
	
	select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
	
	if(FD_ISSET(STDIN_FILENO, &rdfs) == 0)
		return 0;
	else
		return getchar();
	
#else
	return _kbhit() == 0 ? 0 : (int)_getch();
#endif
}
