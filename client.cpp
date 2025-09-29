/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Owen House
	UIN: 434003767
	Date 9/18/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <iostream>

using namespace std;


int main (int argc, char *argv[]) {
	
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	bool new_channel = false;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'c':
				new_channel = true;
				break;
		}
	}

	int status;

	pid_t pid = fork();
	if (pid < 0) { // fork failed
		perror("fork error");
		return -1;
	}
	if (pid == 0){ // run server in child process
		char* args[] = {(char*) "./server", nullptr};
		execvp(args[0], args);
		perror("execvp error");

		exit(0);
	}
	else { // in parent process
		FIFORequestChannel* control_chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
		FIFORequestChannel* active_chan = control_chan;

		if (new_channel) {
			MESSAGE_TYPE m = NEWCHANNEL_MSG;
			control_chan->cwrite(&m, sizeof(MESSAGE_TYPE));

			char new_channel_name[100];
			control_chan->cread(new_channel_name, sizeof(new_channel_name));

			active_chan = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);

			cout << "NEW CHANNEL CREATED" << endl;
		}


		if (filename.empty())
		{
			// example data point request
			char buf[MAX_MESSAGE]; // 256
			datamsg x(p, t, e);
			
			memcpy(buf, &x, sizeof(datamsg));
			active_chan->cwrite(buf, sizeof(datamsg)); // question

			double reply;
			active_chan->cread(&reply, sizeof(double)); //answer
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
			
			string output_path = "received/x1.csv";
			//FILE* output_file = fopen(output_path.c_str(), "a");
			ofstream output_file;
			output_file.open(output_path);

			// part 4.2
			for (int i = 0; i < 1000; i++) {
				char buf1[MAX_MESSAGE];
				datamsg dmsg1(p, i * 0.004, 1); // get ecg_1 value at t = i * 4 ms

				memcpy(buf1, &dmsg1, sizeof(datamsg));
				active_chan->cwrite(buf1, sizeof(datamsg)); // question
				double reply1;
				active_chan->cread(&reply1, sizeof(double)); //answer
				
				char buf2[MAX_MESSAGE];
				datamsg dmsg2(p, i * 0.004, 2); // get ecg_2 value at t = i * 4 ms

				memcpy(buf2, &dmsg2, sizeof(datamsg));
				active_chan->cwrite(buf2, sizeof(datamsg)); // question
				double reply2;
				active_chan->cread(&reply2, sizeof(double)); //answer

				output_file << i * 0.004 << ", " << reply1 << ", " << reply2 << "\n";
			}

			output_file.close();
		}
		else {
			// sending a non-sense message, you need to change this
			filemsg fm(0, 0);
			string fname = filename;
			
			int len = sizeof(filemsg) + (fname.size() + 1);
			char* buf2 = new char[len];
			memcpy(buf2, &fm, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), fname.c_str());
			active_chan->cwrite(buf2, len);  // I want the file length; (request to server)

			__int64_t file_size;
			active_chan->cread(&file_size, sizeof(__int64_t)); // get the file size from the server
			
			delete[] buf2;
			
			string out_path = "received/" + filename;
			FILE* out_file = fopen(out_path.c_str(), "wb");
			if (!out_file) {
				perror("fopen failed");
				exit(1);
			}

			__int64_t remaining_size = file_size;
			__int64_t offset = 0;
			while (remaining_size > 0) {
				// find how many bytes we are reading
				int bytes_to_read;
				if (remaining_size < MAX_MESSAGE) {
					bytes_to_read = remaining_size;
				} 
				else {
					bytes_to_read = MAX_MESSAGE;
				}

				// create message
				filemsg fm(offset, bytes_to_read); // offset, size
				int mlen = sizeof(filemsg) + (fname.size() + 1); // length of meesage
				char* buf = new char[mlen]; // create a buffer
				memcpy(buf, &fm, sizeof(filemsg));
				strcpy(buf + sizeof(filemsg), fname.c_str());

				// send request to server
				active_chan->cwrite(buf, mlen);

				// get reply
				char* reply_buf = new char[bytes_to_read]; // reply buffer
				active_chan->cread(reply_buf, bytes_to_read);
				
				// write to output file
				fwrite(reply_buf, 1, bytes_to_read, out_file);

				// delete buffers and iterate through
				delete[] buf;
				delete[] reply_buf;
				offset += bytes_to_read;
				remaining_size -= bytes_to_read;
			}

			fclose(out_file);
	}
		// closing the channel    
		MESSAGE_TYPE m = QUIT_MSG;
		active_chan->cwrite(&m, sizeof(MESSAGE_TYPE));

		if (new_channel) {
			delete active_chan;
			m = QUIT_MSG;
			control_chan->cwrite(&m, sizeof(MESSAGE_TYPE));
		}

		delete control_chan;

		wait(&status);
	}
}