//
//
//  Please use gcc 7.4.0
//
//
//
//
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include "pds.h"
#include "bst.h"

// Define the global variable
struct PDS_RepoInfo repo_handle;
void build_bst();
static int save_bst_in_file();
static void set_file_names( char *repo_name, char *data_file_name, char *index_file_name );

int pds_open( char *repo_name, int rec_size )
{
  // declare file names
  char data_file_name[sizeof(repo_name)+4], index_file_name[sizeof(repo_name)+4];
  set_file_names( repo_name, data_file_name, index_file_name );

  if( repo_handle.repo_status != PDS_REPO_OPEN )
  {
    // set file pointer
    repo_handle.pds_data_fp = fopen( data_file_name, "ab+" );
    repo_handle.pds_ndx_fp = fopen( index_file_name, "ab+" );

    if( repo_handle.pds_data_fp != NULL && repo_handle.pds_ndx_fp != NULL)
    {
      repo_handle.rec_size = rec_size;
      pds_load_ndx();
      fclose(repo_handle.pds_ndx_fp);
      repo_handle.repo_status = PDS_REPO_OPEN;
      return PDS_SUCCESS;
    }
    else 
      return PDS_FILE_ERROR;
  }
  else
    return PDS_REPO_ALREADY_OPEN;
}

int get_rec_by_ndx_key( int key, void *rec )
{
  if( repo_handle.repo_status == PDS_REPO_OPEN )
  {
    int pk;
    // declare index and contact variables
    struct BST_Node *index;

    // find index from bst using key
    index = bst_search( repo_handle.pds_bst, key );
    if( index != NULL )
    {
      struct PDS_NdxInfo *temp;
      temp = index->data;
      if( temp->is_deleted == 0 )
      {
        // go to offset in data file
        printf("\n\n%d\n\n",temp->offset);
        fseek(repo_handle.pds_data_fp, temp->offset, SEEK_SET);
        fread( &pk,  sizeof(int), 1, repo_handle.pds_data_fp );

        //read contact; (temporary contact variable is not necessary here since non-null index ensures correct search result)
        int status = fread( rec,  repo_handle.rec_size , 1, repo_handle.pds_data_fp );

        if( status == 1 )
          return PDS_SUCCESS;
      }
    }
    return PDS_REC_NOT_FOUND;
  }
  else
    return PDS_FILE_ERROR;
}

int get_rec_by_non_ndx_key(void *key,void *rec,int (*matcher)(void *rec, void *key),int *io_count)
{
  struct BST_Node *index;
  int count = 0;
  int pk;
  if( repo_handle.repo_status == PDS_REPO_OPEN)
  {
    fseek(repo_handle.pds_data_fp, 0, SEEK_SET);
    while(1)
    {
      fread( &pk , sizeof(int), 1, repo_handle.pds_data_fp );
      int status = fread( rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp );
      if( status != 1)
        break;
      count = count + 1;
      if( matcher(rec, key) == 0)
      {
        index = bst_search( repo_handle.pds_bst, pk );
        if( index != NULL )
        {
          struct PDS_NdxInfo *temp;
          temp = index->data;
          if( temp->is_deleted == 0 )
          {
            //*io_count = count;
            return PDS_SUCCESS;
          }
        }
      }
          return PDS_REC_NOT_FOUND;
    }
  }
  else
    return PDS_FILE_ERROR;
}

int put_rec_by_key( int key, void *rec )
{
  if( repo_handle.repo_status == PDS_REPO_OPEN )
  {
    // declare index and status variables
    struct PDS_NdxInfo *index = ( struct PDS_NdxInfo* )malloc( sizeof( struct PDS_NdxInfo ) );
    int offset, write_status, bst_insert_status;

    //seek till eof and get offset
    fseek( repo_handle.pds_data_fp, 0, SEEK_END );
    offset = ftell(repo_handle.pds_data_fp);

    // typecast pointer and write it
    fwrite( &key, sizeof(int) , 1, repo_handle.pds_data_fp);
    write_status = fwrite( rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp);
    if( write_status == 1 )
    {
      // insert index in bst
      index->key = key;
      index->offset = offset;
      index->is_deleted = 0;

      bst_insert_status = bst_add_node( &repo_handle.pds_bst, key, index );
      if( bst_insert_status == BST_SUCCESS )
      {
        return PDS_SUCCESS;
      }
      else 
        return bst_insert_status;
    }
    else 
      return PDS_ADD_FAILED;
  }
  else
    return PDS_FILE_ERROR;
}

int pds_close()
{
  if( repo_handle.repo_status == PDS_REPO_OPEN )
  {
    // set file names 
    char data_file_name[sizeof(repo_handle.pds_name)+4], index_file_name[sizeof(repo_handle.pds_name)+4];
    set_file_names(repo_handle.pds_name, data_file_name, index_file_name);

    //open index file
    repo_handle.pds_ndx_fp = fopen( index_file_name, "wb" );
    fseek( repo_handle.pds_ndx_fp, 0, SEEK_SET );

    if( repo_handle.pds_ndx_fp != NULL)
    {
      //save bst in file
      int bst_status = save_bst_in_file( repo_handle.pds_bst );

      if( bst_status == 1 )
      {
        bst_free( repo_handle.pds_bst );
        repo_handle.pds_bst = NULL;

        int status1 = fclose(repo_handle.pds_data_fp);
        int status2 = fclose(repo_handle.pds_ndx_fp);

        if(status1 == 0 && status2 == 0)
        {
          repo_handle.repo_status = PDS_REPO_CLOSED;
          return PDS_SUCCESS;
        }
      }
    }
  }
  else
    return PDS_FILE_ERROR;
}

int modify_rec_by_key( int key, void *rec )
{
  int write_status;
  if( repo_handle.repo_status == PDS_REPO_OPEN )
  {
    // declare index and contact variables
    struct BST_Node *index;

    // find index from bst using key
    index = bst_search( repo_handle.pds_bst, key );
    if( index != NULL )
    {
      struct PDS_NdxInfo *temp;
      temp = index->data;
      // go to offset in data file
      fseek(repo_handle.pds_data_fp, temp->offset, SEEK_SET);

      fwrite( &key, sizeof(int) , 1, repo_handle.pds_data_fp);
      write_status = fwrite( rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp);

      if( write_status )
        return PDS_SUCCESS;
    }
    else
      return PDS_REC_NOT_FOUND;
  }
  else
    return PDS_FILE_ERROR;
}
int delete_rec_by_ndx_key( int key )
{
  if( repo_handle.repo_status == PDS_REPO_OPEN )
  {
    // declare index and contact variables
    struct BST_Node *index;
    struct PDS_NdxInfo *temp;

    // find index from bst using key
    index = bst_search( repo_handle.pds_bst, key );
    if(index != NULL)
    {
      temp = index->data;
      if(temp->is_deleted == 0)
      {
        temp->is_deleted = 1;
        return PDS_SUCCESS;
      }
    }
    return PDS_DELETE_FAILED;
  }
  else
    return PDS_FILE_ERROR;

}


static void set_file_names( char *repo_name, char *data_file_name, char *index_file_name )
{
  strcpy( repo_handle.pds_name, repo_name );
  strcpy( data_file_name, repo_name );
  strcat( data_file_name, ".dat" );
  strcpy( index_file_name, repo_name );
  strcat( index_file_name, ".ndx" );
}

// use preorder traversal to save file
static int save_bst_in_file( struct BST_Node *root )
{
  if( root == NULL)
    return 1;

  fseek(repo_handle.pds_ndx_fp, 0, SEEK_END);
  struct PDS_NdxInfo *index = (struct PDS_NdxInfo* )root->data;

  if( index != NULL && index->is_deleted == 0)
    fwrite( index, sizeof(struct PDS_NdxInfo), 1, repo_handle.pds_ndx_fp );

  save_bst_in_file( root->left_child );
  save_bst_in_file( root->right_child );
}

int pds_load_ndx()
{
  fseek( repo_handle.pds_ndx_fp, 0, SEEK_SET );
  //create node
  while(1)
  {
    struct PDS_NdxInfo *index = (struct PDS_NdxInfo*)malloc(sizeof(struct PDS_NdxInfo));
    //read into node
    int status = fread( index, sizeof(struct PDS_NdxInfo), 1, repo_handle.pds_ndx_fp );
    if( status == 0)
      break;
    //add node to bst
    bst_add_node( &repo_handle.pds_bst, index->key, index);
  }
}
