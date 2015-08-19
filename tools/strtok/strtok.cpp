#include <string.h>

int main(int argc, char* argv[])
{
if (argc > 0 && strstr(argv[0], "-?", 2) > 0)
{
  showHelp();
  return 0;
}

   char str[80] = "This is - www.tutorialspoint.com - website";
   const char s[2] = "-";
   char *token;
   
   /* get the first token */
   token = strtok(str, s);
   
   /* walk through other tokens */
   while( token != NULL ) 
   {
      printf( " %s\n", token );
    
      token = strtok(NULL, s);
   }
   
   return 0;
}

void showHelp(void)
{
cout << "strtok example project." << endl;
}

