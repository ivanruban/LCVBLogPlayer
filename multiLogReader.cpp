#include <stdint.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>

#include "eventlog.h"
#include "multiLogReader.h"



MultiLogReader::MultiLogReader(std::vector<ILogFile*> &fileList) : files(fileList)
{
   fileContext ctx;
   for(int i =0; i<(int)files.size(); i++)
   {
      contexts.push_back(ctx);
   }
}


/**
 * Reads the next packet.
 * If end of file is reached the 0 is returned.
 *
 * @return amount of read data or -1 on error (errno is set to the error code)
 */
int MultiLogReader::read(packetType &type, timeval &ts, char *data, const int size)
{
   for(int i =0; i<(int)files.size(); i++)
   {
      if(!contexts[i].endIsReached && !contexts[i].valid)
      {
         int err = files[i]->read(contexts[i].type, contexts[i].ts, contexts[i].buff, sizeof(contexts[i].buff));
         if(-1 == err)
         {
            return -1;
         }
         if(0 == err)
         {
            contexts[i].endIsReached = true;
            continue;
         }
         contexts[i].valid = true;
         contexts[i].size = err;
      }
   }

   bool dataReady = false;
   fileContext *EarlierCtx = &contexts[0];
   for(int i =0; i<(int)contexts.size(); i++)
   {
      if(!contexts[i].endIsReached && contexts[i].valid)
      {
         if(timercmp(&EarlierCtx->ts, &contexts[i].ts, >=))
         {
            EarlierCtx = &contexts[i];
            dataReady = true;
         }
      }
   }

   if(dataReady)
   {
      if(EarlierCtx->size > size)
      {
         errno = ENOMEM;
         return -1;
      }

      type = EarlierCtx->type;
      ts = EarlierCtx->ts;
      memcpy(data, EarlierCtx->buff, EarlierCtx->size);
      EarlierCtx->valid = false;
      return EarlierCtx->size;
   }

   return 0;
}

