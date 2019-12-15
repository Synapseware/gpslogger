#include "strtok.h"



int main(int argc, char* argv[])
{
   if (argc < 2)
   {
	  showHelp();
	  return 0;
   }



   char *str = argv[1];
   char *s = "-";
   char *token;

   for (int i = 1; i < argc; i++)
   {
      if (0 == strncmp("-s", argv[i], 2))
         str = argv[++i];
      else if (0 == strncmp("-d", argv[i], 2))
         s = argv[++i];
   }
   
   /* get the first token */
   token = strtok(str, s);
   
   /* walk through other tokens */
   while( token != NULL)
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

