/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer. 
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
			size_t char_offset, size_t *entry_offset_byte_rtn )
{
	//Counter to break out of while loop when requested entry not found
    int counter=0;
    
    //Location to start searching from the first entry
    int buffer_pos = buffer->out_offs;
    
    //Iterate based on the number of entries permissible
    while(counter<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
		//char offset less than size of entry, the requested char must be in this entry
		if(char_offset < buffer->entry[buffer_pos].size)
		{
			//The offset location is same as the updated char offset 
			//and return the entry number
			*entry_offset_byte_rtn = char_offset;
			return &(buffer->entry[buffer_pos]);
		}
		
		//If not found , increment the counter
		counter++;
		
		//Update the offset by decrementing the size of entry that did not have the character requested
		char_offset = char_offset- buffer->entry[buffer_pos].size;
		
		//Updated buffer position to next entry in the buffer and wrap around after reaching end
		buffer_pos = (buffer_pos +1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    //If requested character not found, returns NULL
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
	//Check if the buffer is not NULL
	if(buffer == NULL)
		return;
	
	//Store the buffptr and its size into circular buffer
	buffer->entry[buffer->in_offs] = *add_entry;
	
	//Initially "in" equals "out",yet the buffer is not full and not set as full. After the first entry, every time
	//"in" equals "out" the buffer is set to full. Once the buffer is full, the "out" is incremented by 1 and on
	//reaching the end of circular buffer, just like the "in" , even the "out" is wrapped around
	if(buffer->out_offs != buffer->in_offs)
    {
		buffer->full = false;
    }
    else
    {
		if(buffer->initial == false)
		{
			buffer->full = true;
			buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		}
		else
			buffer->initial=false;
	}
	
	//Increment the buffer position and wrap around if end of buffer is reached
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
    buffer->initial=true;
}
