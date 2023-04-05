#include "file.h"
#include "page.h"

#include <gtest/gtest.h>

#include <string>
#include <iostream>

/*******************************************************************************
 * The test structures stated here were written to give you and idea of what a
 * test should contain and look like. Feel free to change the code and add new
 * tests of your own. The more concrete your tests are, the easier it'd be to
 * detect bugs in the future projects.
 ******************************************************************************/

/*
 * Tests file open/close APIs.
 * 1. Open a file and check the descriptor
 * 2. Check if the file's initial size is 10 MiB
 */
TEST(FileInitTest, HandlesInitialization) {
  int fd;                                 // file descriptor
  std::string pathname = "init_test10.db";  // customize it to your test file

  // Open a database file
  fd = file_open_table_file(pathname.c_str());

  // Check if the file is opened
  ASSERT_TRUE(fd >= 0);  // change the condition to your design's behavior

  // Check the size of the initial file
  int num_pages = /* fetch the number of pages from the header page */ 2560;
  EXPECT_EQ(num_pages, INITIAL_DB_FILE_SIZE / PAGE_SIZE)
      << "The initial number of pages does not match the requirement: "
      << num_pages;

  EXPECT_EQ(2560, file_test_get_np(fd)) << "1-3";
  EXPECT_NE(2559, file_test_get_np(fd)) << "1-4";

  // Check pathname and fd is maintained in disk space manager.
  EXPECT_EQ(get_pathname(fd), pathname.c_str()) << "1-5";

  // Close all database files
  file_close_table_files();

  // Remove the db file
  int is_removed = remove(pathname.c_str());

  ASSERT_EQ(is_removed, /* 0 for success */ 0);

  // Check all the maintained data is deleted in disk space manager.
  EXPECT_EQ(get_pathname(fd), nullptr) << "1-4";
}

/*
 * TestFixture for page allocation/deallocation tests
 */
class FileTest : public ::testing::Test {
 protected:
  /*
   * NOTE: You can also use constructor/destructor instead of SetUp() and
   * TearDown(). The official document says that the former is actually
   * perferred due to some reasons. Checkout the document for the difference
   */
  FileTest() { 
    pathname = "fttmp.db";
    fd = file_open_table_file(pathname.c_str());
    }

  ~FileTest() {
    if (fd >= 0) {
      file_close_table_files();
    }
  }

  int fd;                // file descriptor
  std::string pathname;  // path for the file
};

/*
 * Tests page allocation and free
 * 1. Allocate 2 pages and free one of them, traverse the free page list
 *    and check the existence/absence of the freed/allocated page
 */
TEST_F(FileTest, HandlesPageAllocation) {
  pagenum_t allocated_page, freed_page;
  pagenum_t ffpn, nfn, ffn;
  pagenum_t i;

  // Allocate the pages

  allocated_page = file_alloc_page(fd);
  EXPECT_EQ(allocated_page, 1) << "2-1";
  freed_page = file_alloc_page(fd);
  EXPECT_EQ(freed_page, 2) << "2-2";
  

  // Free one page
  file_free_page(fd, freed_page);

  // Traverse the free page list and check the existence of the freed/allocated
  // pages. You might need to open a few APIs soley for testing.
  ffpn = file_test_get_ffpn(fd);
  EXPECT_EQ(ffpn, 2) << "2-3";

  for (i = ffpn; i != 2559; i = nfn) {
    nfn = file_test_get_nfn(fd, i);
    EXPECT_EQ(nfn, i + 1) << "2-4";
  }
  nfn = file_test_get_nfn(fd, i);
  EXPECT_EQ(nfn, 0) << "2-5";
}

/*
 * Tests page read/write operations
 * 1. Write/Read a page with some random content and check if the data matches
 */
TEST_F(FileTest, CheckReadWriteOperation) {

  pagenum_t pagenum;
  page_t *src, *dest;
  page_t *init;

  try {
    init = new page_t;
    src = new page_t;
    dest = new page_t;
  } catch (std::bad_alloc e) {
    perror(e.what());
    exit(EXIT_FAILURE);
  }

  memset(init, 0, PAGE_SIZE);
  memset(src, 'a', PAGE_SIZE);

  pagenum = file_alloc_page(fd);

  file_read_page(fd, pagenum, dest);
  EXPECT_EQ(memcmp(init, dest, PAGE_SIZE), 0) << "3-1";

  file_write_page(fd, pagenum, src);
  file_read_page(fd, pagenum, dest);
  EXPECT_EQ(memcmp(src, dest, PAGE_SIZE), 0) << "3-2";

  delete init;
  delete src;
  delete dest;
}