/*
 *   Copyright (c) 2023 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:

 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.

 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */


#define PIPEDAL_STATE_URI "http://two-play.com/ns/ext/state"
#define PIPEDAL_STATE_PREFIX "PIPEDAL_STATE_URI#"
#define PIPEDAL_STATE__uploadPath          PIPEDAL_STATE_PREFIX "uploadPath"           ///< http://two-play.com/ns/ext/state#uploadPath


#ifdef __cplusplus
extern "C" {
#endif


typedef void*
  LV2_Pipedal_Upload_Path_Handle; ///< Opaque handle for pipedal:uploadPath feature


/**
   Feature data for state:makePath (@ref LV2_STATE__makePath).
*/
typedef struct {
  /**
     Opaque host data.
  */
  LV2_Pipedal_Upload_Path_Handle handle;

  /**
     Return a path in which upload files will be stored for this plugin.
     @param handle MUST be the `handle` member of this struct.
     @param path The sub-directory name in which uploaded files will be stored.
     @return The absolute path to use for the new directory.

     This function can be used by plugins to pre-populate a writeable Pipedal upload
     directory with sample files.

     The recommended procedure is for the plugin to call this function to get a path
     to a directory. If the directory already exists, assume that the plugin has been 
     previously loaded, so do nothing.

     If the directory does not exist, create it, and create links to files in the 
     plugin's resource directory. This allows users to delete provided files by removing
     the link in the upload directory, since the originals are in a resource directories
     which is not writeable. 

     The caller must free memory for the returned value with LV2_Pipedal_Upload_Path.free_path().
  */
  char* (*upload_path)(LV2_Pipedal_Upload_Path_Handle handle, const char* path);

  /**
    Free a path returned by LV2_Pipedal_Upload_Path.upload_path.
     @param handle MUST be the `handle` member of this struct.
     @param path A path previously returned by LV2_Pipedal_Upload_Path.upload_path().

  */
  void (*free_path)(LV2_Pipedal_Upload_Path_Handle handle, char* path);
} LV2_Pipedal_Upload_Path;
;


#ifdef __cplusplus
} // extern "C"
#endif
