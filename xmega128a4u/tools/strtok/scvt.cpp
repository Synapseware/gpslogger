#include "scvt.h"
#include "waveparser.h"

using namespace std;


void analyzeSoundFile(const char* filename)
{
	FILE * file = fopen(filename, "r");
	if (NULL == file)
	{
		cout << "Could not provide analysis on " << filename << endl;
		return;
	}

	// get the size of the file
	fseek(file, 0, SEEK_END);
	int filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	cout << filename << " is " << filesize << " bytes in length." << endl;
	uint8_t * buffer = (uint8_t*) malloc(filesize * sizeof(uint8_t));

	// read the entire file in
	uint8_t data = 0, largest = 0, smallest = 255;
	while (fread(&data, 1, 1, file) > 0)
	{
		if (smallest > data)
			smallest = data;
		if (largest < data)
			largest = data;

		printf("%d\n", data);
	}
	fclose(file);

	cout << "Largest change: " << largest - smallest << endl;

	//cout << "Loaded " << filesize << " bytes into memory from " << filename << endl;

	free(buffer);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  - -
// Converts a WAV file to an AVW file, the intermediary format
bool convertFile(const char* inputFile)
{
	FILE * file = fopen(inputFile, "r");
	if (NULL == file)
	{
		cout << "Error: Could not open input file " << inputFile << endl;
		return false;
	}

	int len = strlen(inputFile);
	char* outputFile = (char*)malloc(len + 1);
	strcpy(outputFile, inputFile);
	strcpy(outputFile + len - 3, "snd");

	cout << "Input:  " << inputFile << endl;
	cout << "Output: " << outputFile << endl;

	WaveParser * parser = new WaveParser();
	WaveData * data = parser->parse(file);
	if (NULL != data)
	{
		// write the sound data out to disk
		ofstream fout(outputFile, ios::out | ios::binary);
		fout.write(data->buffer(), data->length());
		fout.flush();
		fout.close();
		delete(data);
	}
	else
	{
		cout << "Error: Parsing failed." << endl;
	}

	analyzeSoundFile(outputFile);

	free(outputFile);
	delete(parser);

	return true;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  - -
int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		showHelp();
		return 0;
	}

	char* inputFile = NULL;
	bool analyze = false;
	inputFile = argv[1];

	for (int i = 1; i < argc; i++)
	{
		if (0 == strncmp("-a", argv[i], 2))
			analyze = true;
	}

	if (inputFile == NULL || 0 == strlen(inputFile))
	{
		cout << "Error: missing or invalid input file." << endl;
		return -1;
	}

	// write length of file to output stream
	if (analyze)
		convertFile(inputFile);

	return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  - -
void showHelp(void)
{
	cout << "scvt - wave utility for single file conversion." << endl;
	cout << endl;
	cout << "scvt {input file}" << endl;
	cout << "   Converts a standard wave file to the intermediary Enterprise format, snd" << endl;
	cout << endl;
}
