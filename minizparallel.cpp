#include <cmdline.hpp>
#include <config.hpp>
#include <utilitypar.hpp>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    return -1;
  }
  // parse command line arguments and set some global variables
  long start = parseCommandLine(argc, argv);
  if (start < 0)
    return -1;

  bool success = true;
  double t1, t2;
  t1 = omp_get_wtime();
  while (argv[start]) {
    size_t filesize = 0;
    if (isDirectory(argv[start], filesize)) {
      success &= walkDirParallel(argv[start], COMP);
    } else {
      success &= doParallelWork(argv[start], filesize, COMP);
    }
    start++;
  }
  t2 = omp_get_wtime();
  if (!success) {
    printf("Exiting with (some) Error(s)\n");
    return -1;
  }
  printf("Parallel %f s\n", t2 - t1);
  printf("Exiting with Success\n");
  return 0;
}
