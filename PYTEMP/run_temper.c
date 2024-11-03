#include <stdlib.h>
#include <unistd.h>
int main()
{
    setuid(0); // sudo to root
    system("python3 /home/mark/ASTRO/CURRENT/PYTEMP/temper.py");
    return 0;
}
