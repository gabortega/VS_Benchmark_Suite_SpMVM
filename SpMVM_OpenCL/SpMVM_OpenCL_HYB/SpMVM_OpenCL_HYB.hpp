#include<compiler_config.h>

#if HYB_ELL_SEQ || HYB_ELLG_SEQ || HYB_HLL_SEQ || HYB_ELL || HYB_ELLG || HYB_HLL
#ifndef OPENCL_HYB_H
#define OPENCL_HYB_H

#include<stdio.h>
#include<string>
#include<windows.h>

#include<util_misc.hpp>
#include<CL/cl.h>
#include<JC/util.hpp>
#include<SEQ/ELL.hpp>
#include<SEQ/ELLG.hpp>
#include<SEQ/HLL.hpp>
#include<SEQ/CSR.hpp>

#if PRECISION == 2
#define CL_REAL cl_double
#else
//#elif PRECISION == 1
#define CL_REAL cl_float
//#else
//#define CL_REAL cl_half // TODO?
#endif

#if HYB_ELL_SEQ
std::vector<REAL> spmv_HYB_ELL_sequential(struct hybellg_t* d_hyb, const std::vector<REAL> d_x)
{
	//decrement all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]--;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]--;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]--;
	//
	std::vector<REAL> dst_y(d_x.size(), 0);
	unsigned long long units_REAL = 0, units_IndexType = 0;
	if (d_hyb->ellg.nnz > 0)
	{
		//d_hyb->ellg.a + d_x + dst_y
		units_REAL += 2 * d_hyb->ellg.n * d_hyb->ellg.nell[d_hyb->ellg.n] + d_hyb->ellg.n;
		//d_hyb->ellg.jcoeff
		units_IndexType += d_hyb->ellg.n * d_hyb->ellg.nell[d_hyb->ellg.n];
	}
	if (d_hyb->csr.nnz > 0)
	{
		//d_hyb->csr.a + d_x + dst_y
		units_REAL += d_hyb->csr.nnz + d_hyb->csr.nnz + d_hyb->csr.nnz;
		//d_hyb->csr.ia + d_hyb->csr.ja
		units_IndexType += d_hyb->csr.n + d_hyb->csr.nnz;
	}
	//
	unsigned long long nanoseconds = 0, total_nanoseconds = 0;
	//
	for (int r = 0; r < REPEAT; r++)
	{
		std::fill(dst_y.begin(), dst_y.end(), 0);
		nanoseconds = 0;
		if (d_hyb->ellg.nnz > 0)
		{
			nanoseconds += ELL_sequential(&(d_hyb->ellg), d_x, dst_y);
		}
		if (d_hyb->csr.nnz > 0)
		{
			nanoseconds += CSR_sequential(&(d_hyb->csr), d_x, dst_y);
		}
		printRunInfo(r + 1, nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
		total_nanoseconds += nanoseconds;
	}
	double average_nanoseconds = total_nanoseconds / (double)REPEAT;
	printAverageRunInfo(average_nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
	//increment all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]++;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]++;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]++;

	return dst_y;
}
#endif

#if HYB_ELL
std::vector<CL_REAL> spmv_HYB_ELL(struct hybellg_t* d_hyb, const std::vector<CL_REAL> d_x)
{	
	//decrement all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]--;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]--;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]--;
	//
	IndexType i, row_len = 0, coop = 1, repeat = 1, nworkgroups;
	if (d_hyb->csr.nnz > 0)
	{
		for (i = 0; i < d_hyb->csr.n; i++) row_len += d_hyb->csr.ia[i + 1] - d_hyb->csr.ia[i];
		row_len = sqrt(row_len / d_hyb->csr.n);
		for (coop = 1; coop < 32 && row_len >= coop; coop <<= 1);
		nworkgroups = 1 + (d_hyb->csr.n * coop - 1) / (repeat * CSR_WORKGROUP_SIZE);
		if (nworkgroups > 1500)
			for (repeat = 1; (1 + (d_hyb->csr.n * coop - 1) / ((repeat + 1) * CSR_WORKGROUP_SIZE)) > 1500; repeat++);
		nworkgroups = 1 + (d_hyb->csr.n * coop - 1) / (repeat * CSR_WORKGROUP_SIZE);
	}
	//
	std::vector<CL_REAL> dst_y(d_x.size(), 0);
	unsigned long long units_REAL = 0, units_IndexType = 0;
	if (d_hyb->ellg.nnz > 0)
	{
		//d_hyb->ellg.a + d_x + dst_y
		units_REAL += 2 * d_hyb->ellg.n * d_hyb->ellg.nell[d_hyb->ellg.n] + d_hyb->ellg.n;
		//d_hyb->ellg.jcoeff
		units_IndexType += d_hyb->ellg.n * d_hyb->ellg.nell[d_hyb->ellg.n];
	}
	if (d_hyb->csr.nnz > 0)
	{
		//d_hyb->csr.a + d_x + dst_y
		units_REAL += d_hyb->csr.nnz + d_hyb->csr.nnz + d_hyb->csr.n;
		//d_hyb->csr.ia + d_hyb->csr.ja
		units_IndexType += d_hyb->csr.n + d_hyb->csr.nnz;
	}
	//
	cl::Device device = jc::get_device(CL_DEVICE_TYPE_GPU);
	cl::Context context{ device };
	cl::CommandQueue queue{ context, device, CL_QUEUE_PROFILING_ENABLE };
	//
	//Macros
	std::string csr_macro = "-DPRECISION=" + std::to_string(PRECISION) +
							" -DCSR_REPEAT=" + std::to_string(repeat) +
							" -DCSR_COOP=" + std::to_string(coop) +
							" -DUNROLL_SHARED=" + std::to_string(coop / 4) +
							" -DN_MATRIX=" + std::to_string(d_hyb->csr.n);
	std::string ell_macro = "-DPRECISION=" + std::to_string(PRECISION) + 
							" -DNELL=" + std::to_string(*(d_hyb->ellg.nell + d_hyb->ellg.n)) +
							" -DN_MATRIX=" + std::to_string(d_hyb->ellg.n) +
							" -DSTRIDE_MATRIX=" + std::to_string(d_hyb->ellg.stride);
	//
	cl::Program program_csr =
		jc::build_program_from_file(KERNEL_FOLDER + (std::string)"/" + CSR_KERNEL_FILE, context, device, csr_macro.c_str());
	cl::Kernel kernel_csr{ program_csr, "spmv_csr" };
	//
	cl::Program program_ell =
		jc::build_program_from_file(KERNEL_FOLDER + (std::string)"/" + ELL_KERNEL_FILE, context, device, ell_macro.c_str());
	cl::Kernel kernel_ell{ program_ell, "spmv_ell" };
	//
	std::cout << "CSR kernel macros: " << csr_macro << std::endl << std::endl;
	std::cout << "ELL kernel macros: " << ell_macro << std::endl << std::endl;
	//
	size_t byte_size_d_x = d_x.size() * sizeof(CL_REAL);
	size_t byte_size_dst_y = dst_y.size() * sizeof(CL_REAL);
	//
	cl::Buffer d_x_buffer{ context, CL_MEM_READ_ONLY, byte_size_d_x };
	cl::Buffer dst_y_buffer{ context, CL_MEM_WRITE_ONLY, byte_size_dst_y };
	//
	queue.enqueueWriteBuffer(d_x_buffer, CL_TRUE, 0, byte_size_d_x, d_x.data());
	//
	// ell related
	size_t byte_size_d_jcoeff;
	size_t byte_size_d_a;
	//
	cl::Buffer d_jcoeff_buffer;
	cl::Buffer d_a_buffer;
	//
	if (d_hyb->ellg.nnz > 0)
	{
		byte_size_d_jcoeff = d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n) * sizeof(cl_uint);
		byte_size_d_a = d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n) * sizeof(CL_REAL);
		//
		d_jcoeff_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_jcoeff };
		d_a_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_a };
		//
		queue.enqueueWriteBuffer(d_jcoeff_buffer, CL_TRUE, 0, byte_size_d_jcoeff, d_hyb->ellg.jcoeff);
		queue.enqueueWriteBuffer(d_a_buffer, CL_TRUE, 0, byte_size_d_a, d_hyb->ellg.a);
		//
		kernel_ell.setArg(0, d_jcoeff_buffer);
		kernel_ell.setArg(1, d_a_buffer);
		kernel_ell.setArg(2, d_x_buffer);
		kernel_ell.setArg(3, dst_y_buffer);
	}
	//
	// csr related
	size_t byte_size_d_ia;
	size_t byte_size_d_ja;
	size_t byte_size_d_val;
	//
	cl::Buffer d_ia_buffer;
	cl::Buffer d_ja_buffer;
	cl::Buffer d_val_buffer;
	//
	size_t local_byte_size_shdata;
	//
	if (d_hyb->csr.nnz > 0)
	{
		byte_size_d_ia = (d_hyb->csr.n + 1) * sizeof(cl_uint);
		byte_size_d_ja = d_hyb->csr.nnz * sizeof(cl_uint);
		byte_size_d_val = d_hyb->csr.nnz * sizeof(CL_REAL);
		//
		d_ia_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_ia };
		d_ja_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_ja };
		d_val_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_val };
		//
		cl_ulong size;
		device.getInfo(CL_DEVICE_LOCAL_MEM_SIZE, &size);
		//
		local_byte_size_shdata = CSR_WORKGROUP_SIZE * sizeof(CL_REAL);
		//
		queue.enqueueWriteBuffer(d_ia_buffer, CL_TRUE, 0, byte_size_d_ia, d_hyb->csr.ia);
		queue.enqueueWriteBuffer(d_ja_buffer, CL_TRUE, 0, byte_size_d_ja, d_hyb->csr.ja);
		queue.enqueueWriteBuffer(d_val_buffer, CL_TRUE, 0, byte_size_d_val, d_hyb->csr.a);
		//
		kernel_csr.setArg(0, d_ia_buffer);
		kernel_csr.setArg(1, d_ja_buffer);
		kernel_csr.setArg(2, d_val_buffer);
		kernel_csr.setArg(3, d_x_buffer);
		kernel_csr.setArg(4, dst_y_buffer);
		kernel_csr.setArg(5, cl::Local(local_byte_size_shdata));
		//
		std::cout << "!!! CSR kernel: repeat = " << repeat << ", coop = " << coop << ", nworkgroups = " << nworkgroups << " !!!" << std::endl << std::endl;
		std::cout << "!!! A work-group uses " << local_byte_size_shdata << " bytes of the max local memory size of " << size << " bytes per Compute Unit !!!" << std::endl << std::endl;
	}
	//
	cl_ulong nanoseconds;
	cl_ulong total_nanoseconds = 0;
	//
	for (int r = 0; r < REPEAT; r++)
	{
		nanoseconds = 0;
		queue.enqueueWriteBuffer(dst_y_buffer, CL_TRUE, 0, byte_size_dst_y, dst_y.data());
		if (d_hyb->ellg.nnz > 0)
		{
			nanoseconds +=
				jc::run_and_time_kernel(kernel_ell,
					queue,
					cl::NDRange(jc::best_fit(d_hyb->ellg.n, WORKGROUP_SIZE)),
					cl::NDRange(WORKGROUP_SIZE));
		}
		if (d_hyb->csr.nnz > 0)
		{
			nanoseconds +=
				jc::run_and_time_kernel(kernel_csr,
					queue,
					cl::NDRange(nworkgroups * CSR_WORKGROUP_SIZE),
					cl::NDRange(CSR_WORKGROUP_SIZE));
		}
		printRunInfo(r + 1, nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
		total_nanoseconds += nanoseconds;
	}
	queue.enqueueReadBuffer(dst_y_buffer, CL_TRUE, 0, byte_size_dst_y, dst_y.data());
	double average_nanoseconds = total_nanoseconds / (double)REPEAT;
	printAverageRunInfo(average_nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
	//increment all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]++;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]++;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]++;

	return dst_y;
}
#endif

#if HYB_ELLG_SEQ
std::vector<REAL> spmv_HYB_ELLG_sequential(struct hybellg_t* d_hyb, const std::vector<REAL> d_x)
{
	//decrement all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]--;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]--;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]--;
	//
	std::vector<REAL> dst_y(d_x.size(), 0);
	unsigned long long units_REAL = 0, units_IndexType = 0;
	if (d_hyb->ellg.nnz > 0)
	{
		unsigned long long total_nell;
		IndexType i;
		for (i = 0, total_nell = 0; i < d_hyb->ellg.n; i++) total_nell += d_hyb->ellg.nell[i];
		//d_hyb->ellg.a + d_x + dst_y
		units_REAL += 2 * total_nell + d_hyb->ellg.n;
		//d_hyb->ellg.jcoeff + d_hyb->ellg.nell
		units_IndexType += total_nell + d_hyb->ellg.n;
	}
	if (d_hyb->csr.nnz > 0)
	{
		//d_hyb->csr.a + d_x + dst_y
		units_REAL += d_hyb->csr.nnz + d_hyb->csr.nnz + d_hyb->csr.nnz;
		//d_hyb->csr.ia + d_hyb->csr.ja
		units_IndexType += d_hyb->csr.n + d_hyb->csr.nnz;
	}
	//
	unsigned long long nanoseconds = 0, total_nanoseconds = 0;
	//
	for (int r = 0; r < REPEAT; r++)
	{
		std::fill(dst_y.begin(), dst_y.end(), 0);
		nanoseconds = 0;
		if (d_hyb->ellg.nnz > 0)
		{
			nanoseconds += ELLG_sequential(&(d_hyb->ellg), d_x, dst_y);
		}
		if (d_hyb->csr.nnz > 0)
		{
			nanoseconds += CSR_sequential(&(d_hyb->csr), d_x, dst_y);
		}
		printRunInfo(r + 1, nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
		total_nanoseconds += nanoseconds;
	}
	double average_nanoseconds = total_nanoseconds / (double)REPEAT;
	printAverageRunInfo(average_nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
	//increment all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]++;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]++;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]++;

	return dst_y;
}
#endif

#if HYB_ELLG
std::vector<CL_REAL> spmv_HYB_ELLG(struct hybellg_t* d_hyb, const std::vector<CL_REAL> d_x)
{
	//decrement all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]--;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]--;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]--;
	//
	IndexType i, row_len = 0, coop = 1, repeat = 1, nworkgroups;
	if (d_hyb->csr.nnz > 0)
	{
		for (i = 0; i < d_hyb->csr.n; i++) row_len += d_hyb->csr.ia[i + 1] - d_hyb->csr.ia[i];
		row_len = sqrt(row_len / d_hyb->csr.n);
		for (coop = 1; coop < 32 && row_len >= coop; coop <<= 1);
		nworkgroups = 1 + (d_hyb->csr.n * coop - 1) / (repeat * CSR_WORKGROUP_SIZE);
		if (nworkgroups > 1500)
			for (repeat = 1; (1 + (d_hyb->csr.n * coop - 1) / ((repeat + 1) * CSR_WORKGROUP_SIZE)) > 1500; repeat++);
		nworkgroups = 1 + (d_hyb->csr.n * coop - 1) / (repeat * CSR_WORKGROUP_SIZE);
	}
	//
	std::vector<CL_REAL> dst_y(d_x.size(), 0);
	unsigned long long units_REAL = 0, units_IndexType = 0;
	if (d_hyb->ellg.nnz > 0)
	{
		unsigned long long total_nell;
		IndexType i;
		for (i = 0, total_nell = 0; i < d_hyb->ellg.n; i++) total_nell += d_hyb->ellg.nell[i];
		//d_hyb->ellg.a + d_x + dst_y
		units_REAL += 2 * total_nell + d_hyb->ellg.n;
		//d_hyb->ellg.jcoeff + d_hyb->ellg.nell
		units_IndexType += total_nell + d_hyb->ellg.n;
	}
	if (d_hyb->csr.nnz > 0)
	{
		//d_hyb->csr.a + d_x + dst_y
		units_REAL += d_hyb->csr.nnz + d_hyb->csr.nnz + d_hyb->csr.n;
		//d_hyb->csr.ia + d_hyb->csr.ja
		units_IndexType += d_hyb->csr.n + d_hyb->csr.nnz;
	}
	//
	cl::Device device = jc::get_device(CL_DEVICE_TYPE_GPU);
	cl::Context context{ device };
	cl::CommandQueue queue{ context, device, CL_QUEUE_PROFILING_ENABLE };
	//
	//Macros
	std::string csr_macro = "-DPRECISION=" + std::to_string(PRECISION) +
							" -DCSR_REPEAT=" + std::to_string(repeat) +
							" -DCSR_COOP=" + std::to_string(coop) +
							" -DUNROLL_SHARED=" + std::to_string(coop / 4) +
							" -DN_MATRIX=" + std::to_string(d_hyb->csr.n);
	std::string ellg_macro = "-DPRECISION=" + std::to_string(PRECISION) +
							" -DN_MATRIX=" + std::to_string(d_hyb->ellg.n) +
							" -DSTRIDE_MATRIX=" + std::to_string(d_hyb->ellg.stride);
	//
	cl::Program program_csr =
		jc::build_program_from_file(KERNEL_FOLDER + (std::string)"/" + CSR_KERNEL_FILE, context, device, csr_macro.c_str());
	cl::Kernel kernel_csr{ program_csr, "spmv_csr" };
	//
	cl::Program program_ellg =
		jc::build_program_from_file(KERNEL_FOLDER + (std::string)"/" + ELLG_KERNEL_FILE, context, device, ellg_macro.c_str());
	cl::Kernel kernel_ellg{ program_ellg, "spmv_ellg" };
	//
	std::cout << "CSR kernel macros: " << csr_macro << std::endl << std::endl;
	std::cout << "ELL-G kernel macros: " << ellg_macro << std::endl << std::endl;
	//
	size_t byte_size_d_x = d_x.size() * sizeof(CL_REAL);
	size_t byte_size_dst_y = dst_y.size() * sizeof(CL_REAL);
	//
	cl::Buffer d_x_buffer{ context, CL_MEM_READ_ONLY, byte_size_d_x };
	cl::Buffer dst_y_buffer{ context, CL_MEM_WRITE_ONLY, byte_size_dst_y };
	//
	queue.enqueueWriteBuffer(d_x_buffer, CL_TRUE, 0, byte_size_d_x, d_x.data());
	//
	// ellg related
	size_t byte_size_d_nell;
	size_t byte_size_d_jcoeff;
	size_t byte_size_d_a;
	//
	cl::Buffer d_nell_buffer;
	cl::Buffer d_jcoeff_buffer;
	cl::Buffer d_a_buffer;
	//
	if (d_hyb->ellg.nnz > 0)
	{
		byte_size_d_nell = (d_hyb->ellg.n + 1) * sizeof(cl_uint);
		byte_size_d_jcoeff = d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n) * sizeof(cl_uint);
		byte_size_d_a = d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n) * sizeof(CL_REAL);
		//
		d_nell_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_nell };
		d_jcoeff_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_jcoeff };
		d_a_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_a };
		//
		queue.enqueueWriteBuffer(d_nell_buffer, CL_TRUE, 0, byte_size_d_nell, d_hyb->ellg.nell);
		queue.enqueueWriteBuffer(d_jcoeff_buffer, CL_TRUE, 0, byte_size_d_jcoeff, d_hyb->ellg.jcoeff);
		queue.enqueueWriteBuffer(d_a_buffer, CL_TRUE, 0, byte_size_d_a, d_hyb->ellg.a);
		//
		kernel_ellg.setArg(0, d_nell_buffer);
		kernel_ellg.setArg(1, d_jcoeff_buffer);
		kernel_ellg.setArg(2, d_a_buffer);
		kernel_ellg.setArg(3, d_x_buffer);
		kernel_ellg.setArg(4, dst_y_buffer);
	}
	//
	// csr related
	size_t byte_size_d_ia;
	size_t byte_size_d_ja;
	size_t byte_size_d_val;
	//
	cl::Buffer d_ia_buffer;
	cl::Buffer d_ja_buffer;
	cl::Buffer d_val_buffer;
	//
	size_t local_byte_size_shdata;
	//
	if (d_hyb->csr.nnz > 0)
	{
		byte_size_d_ia = (d_hyb->csr.n + 1) * sizeof(cl_uint);
		byte_size_d_ja = d_hyb->csr.nnz * sizeof(cl_uint);
		byte_size_d_val = d_hyb->csr.nnz * sizeof(CL_REAL);
		//
		d_ia_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_ia };
		d_ja_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_ja };
		d_val_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_val };
		//
		cl_ulong size;
		device.getInfo(CL_DEVICE_LOCAL_MEM_SIZE, &size);
		//
		local_byte_size_shdata = CSR_WORKGROUP_SIZE * sizeof(CL_REAL);
		//
		queue.enqueueWriteBuffer(d_ia_buffer, CL_TRUE, 0, byte_size_d_ia, d_hyb->csr.ia);
		queue.enqueueWriteBuffer(d_ja_buffer, CL_TRUE, 0, byte_size_d_ja, d_hyb->csr.ja);
		queue.enqueueWriteBuffer(d_val_buffer, CL_TRUE, 0, byte_size_d_val, d_hyb->csr.a);
		//
		kernel_csr.setArg(0, d_ia_buffer);
		kernel_csr.setArg(1, d_ja_buffer);
		kernel_csr.setArg(2, d_val_buffer);
		kernel_csr.setArg(3, d_x_buffer);
		kernel_csr.setArg(4, dst_y_buffer);
		kernel_csr.setArg(5, cl::Local(local_byte_size_shdata));
		//
		std::cout << "!!! CSR kernel: repeat = " << repeat << ", coop = " << coop << ", nworkgroups = " << nworkgroups << " !!!" << std::endl << std::endl;
		std::cout << "!!! A work-group uses " << local_byte_size_shdata << " bytes of the max local memory size of " << size << " bytes per Compute Unit !!!" << std::endl << std::endl;
	}
	//
    cl_ulong nanoseconds;
    cl_ulong total_nanoseconds = 0;
    //
	for (int r = 0; r < REPEAT; r++)
	{
		nanoseconds = 0;
		queue.enqueueWriteBuffer(dst_y_buffer, CL_TRUE, 0, byte_size_dst_y, dst_y.data());
		if (d_hyb->ellg.nnz > 0)
		{
			nanoseconds +=
				jc::run_and_time_kernel(kernel_ellg,
					queue,
					cl::NDRange(jc::best_fit(d_hyb->ellg.n, WORKGROUP_SIZE)),
					cl::NDRange(WORKGROUP_SIZE));
		}
		if (d_hyb->csr.nnz > 0)
		{
			nanoseconds +=
				jc::run_and_time_kernel(kernel_csr,
					queue,
					cl::NDRange(nworkgroups * CSR_WORKGROUP_SIZE),
					cl::NDRange(CSR_WORKGROUP_SIZE));
		}
		printRunInfo(r + 1, nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
		total_nanoseconds += nanoseconds;
	}
    queue.enqueueReadBuffer(dst_y_buffer, CL_TRUE, 0, byte_size_dst_y, dst_y.data());
    double average_nanoseconds = total_nanoseconds / (double)REPEAT;
	printAverageRunInfo(average_nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
	//increment all values
	for (IndexType i = 0; i < d_hyb->ellg.stride * *(d_hyb->ellg.nell + d_hyb->ellg.n); i++) d_hyb->ellg.jcoeff[i]++;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]++;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]++;

    return dst_y;
}
#endif

#if HYB_HLL_SEQ
std::vector<REAL> spmv_HYB_HLL_sequential(struct hybhll_t* d_hyb, const std::vector<REAL> d_x)
{
	//decrement all values
	for (IndexType i = 0; i < d_hyb->hll.total_mem; i++) d_hyb->hll.jcoeff[i]--;
	for (IndexType i = 0; i < d_hyb->hll.nhoff; i++) d_hyb->hll.hoff[i]--;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]--;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]--;
	//
	std::vector<REAL> dst_y(d_x.size(), 0);
	unsigned long long units_REAL = 0, units_IndexType = 0;
	if (d_hyb->hll.nnz > 0)
	{
		unsigned long long total_nell;
		IndexType i;
		for (i = 0, total_nell = 0; i < d_hyb->hll.nhoff; i++) total_nell += d_hyb->hll.nell[i];
		//d_hyb->hll.a + d_x + dst_y
		units_REAL += 2 * total_nell + d_hyb->hll.n;
		//d_hyb->hll.jcoeff + d_hyb->hll.nell + d_hyb->hll.hoff
		units_IndexType += total_nell + d_hyb->hll.n + d_hyb->hll.n;
	}
	if (d_hyb->csr.nnz > 0)
	{
		//d_hyb->csr.a + d_x + dst_y
		units_REAL += d_hyb->csr.nnz + d_hyb->csr.nnz + d_hyb->csr.nnz;
		//d_hyb->csr.ia + d_hyb->csr.ja
		units_IndexType += d_hyb->csr.n + d_hyb->csr.nnz;
	}
	//
	unsigned long long nanoseconds = 0, total_nanoseconds = 0;
	//
	for (int r = 0; r < REPEAT; r++)
	{
		std::fill(dst_y.begin(), dst_y.end(), 0);
		nanoseconds = 0;
		if (d_hyb->hll.nnz > 0)
		{
			nanoseconds += HLL_sequential(&(d_hyb->hll), d_x, dst_y);
		}
		if (d_hyb->csr.nnz > 0)
		{
			nanoseconds += CSR_sequential(&(d_hyb->csr), d_x, dst_y);
		}
		printRunInfo(r + 1, nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
		total_nanoseconds += nanoseconds;
	}
	double average_nanoseconds = total_nanoseconds / (double)REPEAT;
	printAverageRunInfo(average_nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
	//increment all values
	for (IndexType i = 0; i < d_hyb->hll.total_mem; i++) d_hyb->hll.jcoeff[i]++;
	for (IndexType i = 0; i < d_hyb->hll.nhoff; i++) d_hyb->hll.hoff[i]++;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]++;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]++;

	return dst_y;
}
#endif

#if HYB_HLL
std::vector<CL_REAL> spmv_HYB_HLL(struct hybhll_t* d_hyb, const std::vector<CL_REAL> d_x)
{
	//decrement all values
	for (IndexType i = 0; i < d_hyb->hll.total_mem; i++) d_hyb->hll.jcoeff[i]--;
	for (IndexType i = 0; i < d_hyb->hll.nhoff; i++) d_hyb->hll.hoff[i]--;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]--;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]--;
	//
	IndexType i, row_len = 0, coop = 1, repeat = 1, nworkgroups;
	if (d_hyb->csr.nnz > 0)
	{
		for (i = 0; i < d_hyb->csr.n; i++) row_len += d_hyb->csr.ia[i + 1] - d_hyb->csr.ia[i];
		row_len = sqrt(row_len / d_hyb->csr.n);
		for (coop = 1; coop < 32 && row_len >= coop; coop <<= 1);
		nworkgroups = 1 + (d_hyb->csr.n * coop - 1) / (repeat * CSR_WORKGROUP_SIZE);
		if (nworkgroups > 1500)
			for (repeat = 1; (1 + (d_hyb->csr.n * coop - 1) / ((repeat + 1) * CSR_WORKGROUP_SIZE)) > 1500; repeat++);
		nworkgroups = 1 + (d_hyb->csr.n * coop - 1) / (repeat * CSR_WORKGROUP_SIZE);
	}
	//
	std::vector<CL_REAL> dst_y(d_x.size(), 0);
	unsigned long long units_REAL = 0, units_IndexType = 0;
	if (d_hyb->hll.nnz > 0)
	{
		unsigned long long total_nell;
		IndexType i;
		for (i = 0, total_nell = 0; i < d_hyb->hll.nhoff; i++) total_nell += d_hyb->hll.nell[i];
		//d_hyb->hll.a + d_x + dst_y
		units_REAL += 2 * total_nell + d_hyb->hll.n;
		//d_hyb->hll.jcoeff + d_hyb->hll.nell + d_hyb->hll.hoff
		units_IndexType += total_nell + d_hyb->hll.n + d_hyb->hll.n;
	}
	if (d_hyb->csr.nnz > 0)
	{
		//d_hyb->csr.a + d_x + dst_y
		units_REAL += d_hyb->csr.nnz + d_hyb->csr.nnz + d_hyb->csr.n;
		//d_hyb->csr.ia + d_hyb->csr.ja
		units_IndexType += d_hyb->csr.n + d_hyb->csr.nnz;
	}
	//
	cl::Device device = jc::get_device(CL_DEVICE_TYPE_GPU);
	cl::Context context{ device };
	cl::CommandQueue queue{ context, device, CL_QUEUE_PROFILING_ENABLE };
	//
	IndexType unroll_val;
	for (unroll_val = 1; (*(d_hyb->hll.nell + d_hyb->hll.nhoff) / 2) >= unroll_val; unroll_val <<= 1);
	//
	//Macros
	std::string csr_macro = "-DPRECISION=" + std::to_string(PRECISION) +
							" -DCSR_REPEAT=" + std::to_string(repeat) +
							" -DCSR_COOP=" + std::to_string(coop) +
							" -DUNROLL_SHARED=" + std::to_string(coop / 4) +
							" -DN_MATRIX=" + std::to_string(d_hyb->csr.n);
	std::string hll_macro = "-DPRECISION=" + std::to_string(PRECISION) +
							" -DHACKSIZE=" + std::to_string(HLL_HACKSIZE) +
							" -DN_MATRIX=" + std::to_string(d_hyb->hll.n) +
							" -DUNROLL=" + std::to_string(unroll_val);
	//
	cl::Program program_csr =
		jc::build_program_from_file(KERNEL_FOLDER + (std::string)"/" + CSR_KERNEL_FILE, context, device, csr_macro.c_str());
	cl::Kernel kernel_csr{ program_csr, "spmv_csr" };
	//
	cl::Program program_hll =
		jc::build_program_from_file(KERNEL_FOLDER + (std::string)"/" + HLL_KERNEL_FILE, context, device, hll_macro.c_str());
	cl::Kernel kernel_hll{ program_hll, "spmv_hll" };
	//
	std::cout << "CSR kernel macros: " << csr_macro << std::endl << std::endl;
	std::cout << "HLL kernel macros: " << hll_macro << std::endl << std::endl;
	//
	size_t byte_size_d_x = d_x.size() * sizeof(CL_REAL);
	size_t byte_size_dst_y = dst_y.size() * sizeof(CL_REAL);
	//
	cl::Buffer d_x_buffer{ context, CL_MEM_READ_ONLY, byte_size_d_x };
	cl::Buffer dst_y_buffer{ context, CL_MEM_WRITE_ONLY, byte_size_dst_y };
	//
	queue.enqueueWriteBuffer(d_x_buffer, CL_TRUE, 0, byte_size_d_x, d_x.data());
	//
	// hll related
	size_t byte_size_d_nell;
	size_t byte_size_d_jcoeff;
	size_t byte_size_d_hoff;
	size_t byte_size_d_a;
	//
	cl::Buffer d_nell_buffer;
	cl::Buffer d_jcoeff_buffer;
	cl::Buffer d_hoff_buffer;
	cl::Buffer d_a_buffer;
	//
	if (d_hyb->hll.nnz > 0)
	{
		byte_size_d_nell = d_hyb->hll.nhoff * sizeof(cl_uint);
		byte_size_d_jcoeff = d_hyb->hll.total_mem * sizeof(cl_uint);
		byte_size_d_hoff = d_hyb->hll.nhoff * sizeof(cl_uint);
		byte_size_d_a = d_hyb->hll.total_mem * sizeof(CL_REAL);
		//
		d_nell_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_nell };
		d_jcoeff_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_jcoeff };
		d_hoff_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_hoff };
		d_a_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_a };
		//
		queue.enqueueWriteBuffer(d_nell_buffer, CL_TRUE, 0, byte_size_d_nell, d_hyb->hll.nell);
		queue.enqueueWriteBuffer(d_jcoeff_buffer, CL_TRUE, 0, byte_size_d_jcoeff, d_hyb->hll.jcoeff);
		queue.enqueueWriteBuffer(d_hoff_buffer, CL_TRUE, 0, byte_size_d_hoff, d_hyb->hll.hoff);
		queue.enqueueWriteBuffer(d_a_buffer, CL_TRUE, 0, byte_size_d_a, d_hyb->hll.a);
		//
		kernel_hll.setArg(0, d_nell_buffer);
		kernel_hll.setArg(1, d_jcoeff_buffer);
		kernel_hll.setArg(2, d_hoff_buffer);
		kernel_hll.setArg(3, d_a_buffer);
		kernel_hll.setArg(4, d_x_buffer);
		kernel_hll.setArg(5, dst_y_buffer);
	}
	//
	// csr related
	size_t byte_size_d_ia;
	size_t byte_size_d_ja;
	size_t byte_size_d_val;
	//
	cl::Buffer d_ia_buffer;
	cl::Buffer d_ja_buffer;
	cl::Buffer d_val_buffer;
	//
	size_t local_byte_size_shdata;
	//
	if (d_hyb->csr.nnz > 0)
	{
		byte_size_d_ia = (d_hyb->csr.n + 1) * sizeof(cl_uint);
		byte_size_d_ja = d_hyb->csr.nnz * sizeof(cl_uint);
		byte_size_d_val = d_hyb->csr.nnz * sizeof(CL_REAL);
		//
		d_ia_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_ia };
		d_ja_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_ja };
		d_val_buffer = cl::Buffer{ context, CL_MEM_READ_ONLY, byte_size_d_val };
		//
		cl_ulong size;
		device.getInfo(CL_DEVICE_LOCAL_MEM_SIZE, &size);
		//
		local_byte_size_shdata = CSR_WORKGROUP_SIZE * sizeof(CL_REAL);
		//
		queue.enqueueWriteBuffer(d_ia_buffer, CL_TRUE, 0, byte_size_d_ia, d_hyb->csr.ia);
		queue.enqueueWriteBuffer(d_ja_buffer, CL_TRUE, 0, byte_size_d_ja, d_hyb->csr.ja);
		queue.enqueueWriteBuffer(d_val_buffer, CL_TRUE, 0, byte_size_d_val, d_hyb->csr.a);
		//
		kernel_csr.setArg(0, d_ia_buffer);
		kernel_csr.setArg(1, d_ja_buffer);
		kernel_csr.setArg(2, d_val_buffer);
		kernel_csr.setArg(3, d_x_buffer);
		kernel_csr.setArg(4, dst_y_buffer);
		kernel_csr.setArg(5, cl::Local(local_byte_size_shdata));
		//
		std::cout << "!!! CSR kernel: repeat = " << repeat << ", coop = " << coop << ", nworkgroups = " << nworkgroups << " !!!" << std::endl << std::endl;
		std::cout << "!!! A work-group uses " << local_byte_size_shdata << " bytes of the max local memory size of " << size << " bytes per Compute Unit !!!" << std::endl << std::endl;
	}
	//
	cl_ulong nanoseconds;
	cl_ulong total_nanoseconds = 0;
	//
	for (int r = 0; r < REPEAT; r++)
	{
		nanoseconds = 0;
		queue.enqueueWriteBuffer(dst_y_buffer, CL_TRUE, 0, byte_size_dst_y, dst_y.data());
		if (d_hyb->hll.nnz > 0)
		{
			nanoseconds +=
				jc::run_and_time_kernel(kernel_hll,
					queue,
					cl::NDRange(jc::best_fit(d_hyb->hll.n, WORKGROUP_SIZE)),
					cl::NDRange(WORKGROUP_SIZE));
		}
		if (d_hyb->csr.nnz > 0)
		{
			nanoseconds +=
				jc::run_and_time_kernel(kernel_csr,
					queue,
					cl::NDRange(nworkgroups * CSR_WORKGROUP_SIZE),
					cl::NDRange(CSR_WORKGROUP_SIZE));
		}
		printRunInfo(r + 1, nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
		total_nanoseconds += nanoseconds;
	}
	queue.enqueueReadBuffer(dst_y_buffer, CL_TRUE, 0, byte_size_dst_y, dst_y.data());
	double average_nanoseconds = total_nanoseconds / (double)REPEAT;
	printAverageRunInfo(average_nanoseconds, (d_hyb->nnz), units_REAL, units_IndexType);
	//increment all values
	for (IndexType i = 0; i < d_hyb->hll.total_mem; i++) d_hyb->hll.jcoeff[i]++;
	for (IndexType i = 0; i < d_hyb->hll.nhoff; i++) d_hyb->hll.hoff[i]++;
	for (IndexType i = 0; i < d_hyb->csr.n + 1; i++) d_hyb->csr.ia[i]++;
	for (IndexType i = 0; i < d_hyb->csr.nnz; i++) d_hyb->csr.ja[i]++;

	return dst_y;
}
#endif

#endif
#endif