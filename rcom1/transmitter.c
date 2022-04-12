#include "transmitter.h"

//static struct termios oldtio, newtio;
static int tx_fd;
u_int8_t timeoutFlag, timerFlag, timeoutCount;
linkLayer tx_cParameters;

//static int sequenceBit = 0;

int transmitter_llopen(linkLayer connectionParameters){
    //*tx_cParameters = connectionParameters;
    tx_fd = configureSerialterminal(connectionParameters);

    // SET frame header
    u_int8_t cmdSet[] = {FLAG, A_tx, C_SET, A_tx ^ C_SET, FLAG};
    // UA frame header, what we are expecting to receive
    u_int8_t cmdUA[] = {FLAG, A_tx, C_UA, A_tx ^ C_UA, FLAG};

    (void) signal(SIGALRM, timeOut);

    int res = write(tx_fd, cmdSet, 5);
    if(res < 0){
        perror("Error writing to serial port");
        return NULL;
    }
    printf("%d bytes written\n", res);

    timeoutFlag = 0, timeoutCount = 0, timerFlag = 1;

    while (timeoutCount < connectionParameters.timeOut){
        if(timerFlag){
            alarm(3);
            timerFlag = 0;
        }

        int readResult = getCommand(tx_fd, cmdUA, 5);

        if(readResult < 0){
            printf("Error reading command from serial port");
            return NULL;
        }
        else if(readResult > 0){ //Success
            signal(SIGALRM, SIG_IGN); //disable interrupt handler
            printf("Received UA, connection established\n");
            return 1;
        }

        if(timeoutFlag){
            int res = write(tx_fd, cmdSet, 5);
            if(res < 0){
                perror("Error writing to serial port");
                return NULL;
            }
            printf("%d bytes written\n", res);
            timeoutCount++;
        }
    }    

    return NULL;
}

u_int8_t *prepareInfoFrame(char *buf, int bufSize, int *outputSize, u_int8_t sequenceBit){
    if(buf == NULL || bufSize <= 0 || bufSize > MAX_PAYLOAD_SIZE)
        return NULL;

    // Prepare the frame data
    u_int8_t *data = malloc(bufSize + 1);
    if(data == NULL)
        return NULL;
    
    for (int i = 0; i < bufSize; i++) // Copy the buffer
        data[i] = buf[i];
    // Add the BCC byte
    data[bufSize] = (u_int8_t) generateBCC(buf, bufSize);

    int stuffedSize = 0;
    u_int8_t *stuffedData = byteStuffing(data, bufSize+1, &stuffedSize);
    if(stuffedData == NULL)
        return NULL;
    
    free(data);

    u_int8_t *outgoingData = malloc(stuffedSize + 5);
    if(outgoingData == NULL){
        free(stuffedData);
        return NULL;
    }
    outgoingData[0] = FLAG;
    outgoingData[1] = A_tx;
    outgoingData[2] = C(sequenceBit);
    outgoingData[3] = A_tx ^ C(sequenceBit);

    // Copy the stuffed data
    for (int i = 0; i < stuffedSize; i++)
        outgoingData[i+4] = stuffedData[i];
    
    outgoingData[stuffedSize + 4] = FLAG;

    free(stuffedData);

    *outputSize = stuffedSize + 5;
    return outgoingData;
}

int llwrite(char *buf, int bufSize){
    int frameSize = 0;
    u_int8_t frame = prepareInfoFrame(buf, bufSize, &frameSize, 0);

    timeoutFlag = 0; timerFlag = 1; timeoutCount;
    while (timeoutCount <= MAX_RETRANSMISSIONS_DEFAULT)
    {
        
    }
    
    return 0;
}

/* int transmitter_llclose(linkLayer connectionParameters){

    // DISC frame header
    u_int8_t cmdDisc[] = {FLAG, A_tx, C_DISC, A_tx ^ C_DISC, FLAG};
    // UA frame header
    u_int8_t cmdUA[] = {FLAG, A_rx, C_UA, A_rx ^ C_UA, FLAG};

    (void) signal(SIGALRM, timeOut);

    int res = write(tx_fd, cmdDisc, 5);
    if(res < 0){
        perror("Error writing to serial port");
        return NULL;
    }
    printf("Disconnect command sent\n");

    int timeoutCount = 0;

    timeoutFlag = 0, timeoutCount = 0, timerFlag = 1;

    while (timeoutCount < connectionParameters.timeOut){
        if(timerFlag){
            alarm(3);
            timerFlag = 0;
        }

        int readResult = getCommand(tx_fd, cmdDisc, 5);

        if(readResult < 0){
            perror("Error reading command from serial port");
            return NULL;
        }
        else if(readResult > 0){ //Success
            signal(SIGALRM, SIG_IGN); //disable interrupt handler
            printf("Received Disconnection confirm, sending UA\n");
            break;
        }

        if(timeoutFlag){
            int res = write(tx_fd, cmdDisc, 5);
            if(res < 0){
                perror("Error writing to serial port");
                return NULL;
            }
            printf("Disconnect command sent again\n");
            timeoutCount++;
        }
    }    
    
    if(write(tx_fd, cmdUA, 5) < 0){
        perror("Error writing to serial port");
        return NULL;
    }
    printf("UA control sent\n");
    return 1;
} */

u_int8_t *byteStuffing(u_int8_t *data, int dataSize, int *outputDataSize){
    if(data == NULL || outputDataSize == NULL){
        printf("one or more parameters are invalid\n");
        return NULL;
    }

    // Maximum possible stuffed data size is twice that of the input data array
    // We prevent having to reallocate memory during the stuffing
    u_int8_t *stuffedData = malloc(2*dataSize); 
    if(stuffedData == NULL)
        return NULL;
    
    int size = 0;

    for (int i = 0; i < dataSize; i++){
        switch (data[i])
        {
        case FLAG:
            stuffedData[size++] = ESC;
            stuffedData[size++] = FLAG ^ 0x20;
            break;
        case ESC:
            stuffedData[size++] = ESC;
            stuffedData[size++] = ESC ^ 0x20;  
            break;
        default:
            stuffedData[size++] = data[i];
            break;
        }
    }

    // Trim the array in memory if needed
    if(size != 2*dataSize){
        stuffedData = realloc(stuffedData, size);
        if(stuffedData == NULL)
            return NULL;
    }
    *outputDataSize = size;
    return stuffedData;
}

void timeOut(){
    printf("Connection timeout\n");
    timeoutFlag = 1;    //indicate there was a timeout
    timerFlag = 1;      //restart the timer 
}