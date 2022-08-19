#include <stdio.h>
#include <omp.h>
#include <cstdlib>
#include <cuda_runtime.h>

// Testing cudaMemcpy -> Used for testing different array sizes -> no validation 

int arr_len = 10;
// int arr_len = 256; // 1 Kb
// int arr_len = 2560; // 10 Kb
// int arr_len = 25600; // 100 Kb
// int arr_len = 262144; // 1 Mb
// int arr_len = 2621440; // 10 Mb
// int arr_len = 26214400; // 100 Mb
// int arr_len = 268435456; // 1 Gb

bool v_flag = false; // Print verifications True=On False=Off

void verify(void ** x_ptr){
  int num_dev = omp_get_num_devices();  
  for(int i=0; i<num_dev; ++i)
  {
    #pragma omp target device(i) map(x_ptr[i]) 
    {
      int* x = (int*)x_ptr[i];
      printf("%s No. %d ArrX = ", omp_is_initial_device()?"Host":"Device", omp_get_device_num());
      for (int j=0; j<arr_len; ++j){
        printf("%d ", *(x+j));
      }
      printf("\n");
    }
  }
}

void print_hval(int * x_arr){
  printf("Host -> Value of X: ");
  for (int j=0; j<arr_len; ++j)
    printf("%d ", x_arr[j]);
  printf("\n");
}

int main()
{
  double start, end; 
  int num_dev = omp_get_num_devices();  
  int* x_arr = (int*) malloc(arr_len * sizeof(int)); 
  
  cudaError_t c_error;

  // Check PeerToPeer
  int can_access = 0;
  for(int dev_a = 0; dev_a < num_dev; ++dev_a){
    for(int dev_b = num_dev-1; dev_b >= 0; --dev_b){
    // for(int dev_b = 0; dev_b < num_dev; ++dev_b){
      if (dev_a != dev_b){
        cudaDeviceCanAccessPeer(&can_access, dev_a, dev_b);
        if(!can_access){
          cudaSetDevice(dev_a);
          c_error = cudaDeviceEnablePeerAccess(dev_b, 0);
          cudaDeviceCanAccessPeer(&can_access, dev_a, dev_b);
          if(!can_access){
            printf("Unable to enable P2P Dev: %d -> Dev: %d\n", dev_a, dev_b);
            printf("code: %d, reason: %s\n", c_error, cudaGetErrorString(c_error));
          }else{
            printf("Enabled P2P Dev: %d -> Dev: %d\n", dev_a, dev_b);
          }
        }else{
            cudaSetDevice(dev_a);
            c_error = cudaDeviceEnablePeerAccess(dev_b, 0);
            printf("Default: (%d) Enabled P2P Dev: %d -> Dev: %d\n",c_error, dev_a, dev_b);
        }
      }
    }
  }

  // Set device pointers
  void ** x_ptr = (void **) malloc(sizeof(void**) * num_dev+1); 
  if (!x_ptr) {
    printf("Memory Allocation Failed\n");
    exit(1);
  }
  size_t size = sizeof(int) * arr_len;

  // Allocate device memory
  for (int dev = 0; dev < num_dev; ++dev){
    cudaSetDevice(dev);
    cudaMalloc(&x_ptr[dev], size);
  }
  // Add host pointer 
  x_ptr[num_dev]=&x_arr[0];

  printf("[Broadcast Int Array Size = %zu]\n", size);
  printf("No. of Devices: %d\n", omp_get_num_devices());
  
//**************************************************//
//                   Host-to-All                    //
//**************************************************//
/*
  for (int i=0; i<arr_len; ++i)
    x_arr[i]=i+10;

  // printf("Host-to-All\n");
  if (v_flag) print_hval(x_arr);

  start = omp_get_wtime(); 
  #pragma omp parallel num_threads(omp_get_num_devices()) shared(x_ptr)
  {
    cudaMemcpy(
      x_ptr[omp_get_thread_num()],        // dst
      x_ptr[omp_get_initial_device()],    // src
      size,                               // length 
      cudaMemcpyHostToDevice              // kind
    );
  }
  end = omp_get_wtime();
  // printf( "%f seconds <<<<< Host-to-All\n", end - start);
  printf( "%f\n", end - start);
  if (v_flag) verify(x_ptr);

*/
//**************************************************//
//            Host-to-One -> One-to-All             //
//**************************************************//

  for (int i=0; i<arr_len; ++i)
    x_arr[i]=i+20;

  // printf("Host-to-One -> One-to-All\n");
  if (v_flag) print_hval(x_arr);
  start = omp_get_wtime(); 
  #pragma omp parallel num_threads(omp_get_num_devices()) shared(x_ptr)
  {
    #pragma omp single
    {
      int dependency;
      #pragma omp task depend(out:dependency)
        cudaMemcpy(
          x_ptr[0],                           // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      for(int i=1; i<num_dev; ++i)
        #pragma omp task depend(in:dependency) firstprivate(i) private (c_error)
        {
          // cudaSetDevice(dev);
          c_error = cudaMemcpyPeer(
            x_ptr[i],                         // dst ptr
            i,                                // dst_device_num
            x_ptr[0],                         // src ptr
            0,                                // src ptr_device_num
            size                              // length 
          );
          printf("code: %d, reason: %s\n", c_error, cudaGetErrorString(c_error));
        }
    }
  }
  #pragma omp taskwait
  end = omp_get_wtime();
  // printf( "%f seconds <<<<< Host-to-One -> One-to-All\n", end - start);
  printf( "%f\n", end - start);
  if (v_flag) verify(x_ptr);


//**************************************************//
//            Host-to-One -> Binary tree            //
//**************************************************//
/*
  for (int i=0; i<arr_len; ++i)
    x_arr[i]=i+30;

  // printf("Host-to-One -> Binary tree\n");
  if (v_flag) print_hval(x_arr);
  start = omp_get_wtime(); 
  #pragma omp parallel num_threads(omp_get_num_devices()) shared(x_ptr)
  {
    #pragma omp single
    {
      int dep_arr[num_dev];
      #pragma omp task depend(out:dep_arr[0])
        cudaMemcpy(
          x_ptr[0],                           // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      for(int i=1; i<num_dev; ++i)
        #pragma omp task depend(in:dep_arr[(i-1)/2]) depend(out:dep_arr[i]) firstprivate(i)
          // cudaSetDevice(dev);
          cudaMemcpy(
            x_ptr[i],                         // dst
            x_ptr[(i-1)/2],                   // src
            size,                             // length 
            cudaMemcpyDeviceToDevice          // kind
          );
    }
  }
  #pragma omp taskwait
  end = omp_get_wtime();
  // printf( "%f seconds <<<<< Host-to-one -> Binary tree\n", end - start);
  printf( "%f\n", end - start);
  if (v_flag) verify(x_ptr);

*/
//**************************************************//
//               Host-to-Binary Tree                //
//**************************************************//
/*
  for (int i=0; i<arr_len; ++i)
    x_arr[i]=i+40;
  
  // printf("Host-to-Binary Tree\n");
  if (v_flag) print_hval(x_arr);
  start = omp_get_wtime();  
  #pragma omp parallel num_threads(omp_get_num_devices()) shared(x_ptr)
  {
    #pragma omp single
    {
      int dep_arr[num_dev];
      #pragma omp task depend(out:dep_arr[0])
        cudaMemcpy(
          x_ptr[0],                           // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      #pragma omp task depend(out:dep_arr[1])
        // cudaSetDevice(dev);
        cudaMemcpy(
          x_ptr[1],                           // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      for(int i=2; i<num_dev; ++i)
        #pragma omp task depend(in:dep_arr[(i-1)/2]) depend(out:dep_arr[i]) firstprivate(i)
        // cudaSetDevice(dev);
        cudaMemcpy(
          x_ptr[i],                           // dst
          x_ptr[(i-1)/2],                     // src
          size,                               // length 
          cudaMemcpyDeviceToDevice            // kind
        );
    }
  }
  #pragma omp taskwait
  end = omp_get_wtime();
  // printf( "%f seconds <<<<< Host-to-Binary Tree\n", end - start);
  printf( "%f\n", end - start);
  if (v_flag) verify(x_ptr);

*/
//**************************************************//
//            Host-to-One -> Linked List            //
//**************************************************//
/*
  for (int i=0; i<arr_len; ++i)
    x_arr[i]=i+50;
  
  // printf("Host-to-one -> Linked List\n");
  if (v_flag) print_hval(x_arr);
  start = omp_get_wtime();  
  #pragma omp parallel num_threads(omp_get_num_devices()) shared(x_ptr)
  {
    #pragma omp single
    {
      int dep_arr[num_dev];
      #pragma omp task depend(out:dep_arr[0])
        cudaMemcpy(
          x_ptr[0],                           // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      for(int i=1; i<num_dev; ++i)
        #pragma omp task depend(in:dep_arr[i-1]) depend(out:dep_arr[i]) firstprivate(i)
        cudaMemcpy(
          x_ptr[i],                           // dst
          x_ptr[i-1],                         // src
          size,                               // length 
          cudaMemcpyDeviceToDevice            // kind
        );
    }
  }
  #pragma omp taskwait
  end = omp_get_wtime();
  // printf("%f seconds <<<<< Host-to-one -> Linked List\n", end - start);
  printf( "%f\n", end - start);
  if (v_flag) verify(x_ptr);
*/
//**************************************************//
//           Host-to-Splited Linked List            //
//**************************************************//
/*
  for (int i=0; i<arr_len; ++i)
    x_arr[i]=i+60;
  
  // printf("Host-to-Splited Linked List\n");
  if (v_flag) print_hval(x_arr);
  start = omp_get_wtime();  
  #pragma omp parallel num_threads(omp_get_num_devices()) shared(x_ptr)
  {
    #pragma omp single
    {
      int dep_arr[num_dev];
      #pragma omp task depend(out:dep_arr[0])
        cudaMemcpy(
          x_ptr[0],                           // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      #pragma omp task depend(out:dep_arr[num_dev/2])
        cudaMemcpy(
          x_ptr[num_dev/2],                   // dst
          x_ptr[omp_get_initial_device()],    // src
          size,                               // length 
          cudaMemcpyHostToDevice              // kind
        );
      for(int i=1; i<num_dev/2; ++i){
        #pragma omp task depend(in:dep_arr[i-1]) depend(out:dep_arr[i]) firstprivate(i)
        cudaMemcpy(
          x_ptr[i],                           // dst
          x_ptr[i-1],                         // src
          size,                               // length 
          cudaMemcpyDeviceToDevice            // kind
        );
        #pragma omp task depend(in:dep_arr[num_dev/2+i-1]) depend(out:dep_arr[num_dev/2+i]) firstprivate(i)
        cudaMemcpy(
          x_ptr[num_dev/2+i],                 // dst
          x_ptr[num_dev/2+i-1],               // src
          size,                               // length 
          cudaMemcpyDeviceToDevice            // kind
        );
      }
    }
  }
  #pragma omp taskwait
  end = omp_get_wtime();
  // printf("%f seconds <<<<< Host-to-one -> Splited Linked List\n", end - start);
  printf( "%f\n", end - start);
  if (v_flag) verify(x_ptr);
*/
  free(x_ptr);

  return 0;
}


// CUDA memory copy types

// Enumerator:
// cudaMemcpyHostToHost 	Host -> Host
// cudaMemcpyHostToDevice 	Host -> Device
// cudaMemcpyDeviceToHost 	Device -> Host
// cudaMemcpyDeviceToDevice 	Device -> Device
// cudaMemcpyDefault 	Default based unified virtual address space


//gcc -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart

// PROG=./tests/broadcast_14; clang++ -fopenmp -fopenmp-targets=nvptx64 -o $PROG.x --cuda-gpu-arch=sm_70 -L/soft/compilers/cuda/cuda-11.0.2/lib64 -L/soft/compilers/cuda/cuda-11.0.2/targets/x86_64-linux/lib/ -I/soft/compilers/cuda/cuda-11.0.2/include -ldl -lcudart -pthread $PROG.cpp
// ./