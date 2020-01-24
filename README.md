# Memory Compression Measurement

This project is a extensible and multithreaded file (memory dump) compression report program.

To compile, run **$ make**,  
**$ make bootstrap driver** or **$ make \<COMPRESSION_NAME\>** or **$ make \<MEMORY_LAYOUT_NAME\>** to compile specific .c file.

Use **$ make list** to see what is available.

For a minimal run, execute **$ ./bin/driver -f %filename%**   
To see flag description, run **$ ./bin/driver**
```
Usage: ./driver [-f filename] [-n thread_count] [-v] [-p] [-z] [-h] [-l]
Where -v is for validation (check decompression).
      -z if you want statistic with zero pages. (by default zero pages are skipped)
      -p will allow compressed size to be larger than compressed unit.
      -h removes report header.
      -n thread count, default is 4 threads (Use hardware thread count + 1 threads for best performance)
      -l run without layouts
      -a instead of ratio, print compressed size in bits
```
A python script is provided to run the program. Make edits according to the script to run the program.


Legacy: 
```
This main program uses multiple threads and therefore tasks (files) should be queued instead of run in parallel on one machine.     
To queue tasks, run in bash:

for filename in /Path/to/dumpfiles/*;
do
    ./bin/driver -f "$filename" >> report.csv;
done

In the end, a csv formated output will be generated. Compression ratio (uncompressed/compressed) will be reported.  
Use a spread sheet program to copy results for one column.

All .so files in `bin/compressions/` and `bin/layouts/` folders will be run. Make sure you don't have unnecessary .so files.
```

---

## Description

### **Compressions**

Compressions are for actrual compression work; code that only do calculation should not be included here. 
4KB Pages will be compressed in each method and result size (in bits) will be reported.
By cacheline compression will try to write a by-cacheline compressed size report for each page, so that some layout can take the result for computation.
The driver then calculate the total bits used and write to the final report. 

##### cpack
Code of cpack was originally obtained from [*internet*](https://github.com/benschreiber/cpack) (under MIT license?).     
Cpack is a by-cacheline compression method. See [*paper*](http://ieeexplore.ieee.org/document/5229354/) for details.

##### bdi
Bdi is also a by-cacheline compression method.          
See [*paper*](https://users.ece.cmu.edu/~omutlu/pub/bdi-compression_pact12.pdf) for details.

##### bpc
BPC is Bit Plane Compression and is not a by-cacheline compression method.  
A by-cacheline version is avaliable in Compresso layout.       
See [*paper*](https://dl.acm.org/citation.cfm?id=3001172) for details

##### bpc_compresso
Modified BPC based on compresso paper.     
See paper of compresso in layout section for details

##### deflate
Deflate uses original code from [*zlib*](https://zlib.net/) under its own license.    
Not a by-cacheline compression, or, will compress by page.

##### lz4
Lz4 compression by page.     
See [*link*](https://github.com/lz4/lz4) for details.

##### huffman1byte
A simple huffman compression. Each literal is a byte.


### **Layout**
Layouts doesn't comrpess the data, they simulate how data is stored.   
Usually layouts are for faster memory accesses.     
Hence, layouts only provide measurement number for non-zero pages and does not support validation.

##### best-of  
As the title suggested. Find the ratio if multiple compressions applied at same time.

##### binaryization
As the title suggestes, this layout allows pages either uncompressed or compressed so
the page has a compressed size less than a specific value, and take that value as the compressed size. This layout is used to find out what percentage of page can be compressed
so they are bounded by which compressed size.

##### Compresso
Compresso uses a modified by-cacheline BPC, allows some granularized cacheline and page sizes to speed up address translation, and has 64B/Page metadata overhead. 
See [*paper*](lph.ece.utexas.edu/merez/uploads/MattanErez/micro18_compresso.pdf)    
Here we simuate the best situation compression ratio for Compresso by ignoring the dynamic part.    

---

## Compilation / Make Rules
During compilation,        
c files and folders with Makefile in `src/compressions` will be automatically compiled to `bin/compressions/*.so`, and    
c files and folders with Makefile in `src/layouts` will be automatically compiled to `bin/layouts/*.so`    
If you want to do something different than just one .c file, use a folder in place of the file. Put all of your source code in the folder and write your own Makefile. 
