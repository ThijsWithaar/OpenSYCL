/*
 * This file is part of AdaptiveCpp, an implementation of SYCL and C++ standard
 * parallelism for CPUs and GPUs.
 *
 * Copyright The AdaptiveCpp Contributors
 *
 * AdaptiveCpp is released under the BSD 2-Clause "Simplified" License.
 * See file LICENSE in the project root for full license details.
 */
 // SPDX-License-Identifier: BSD-2-Clause

#include <map>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <span>
#include <set>
#include <format>
#include <variant>

#include <fmt/format.h>


#include "hipSYCL/runtime/application.hpp"
#include "hipSYCL/runtime/backend.hpp"
#include "hipSYCL/runtime/runtime.hpp"
#include "hipSYCL/runtime/hardware.hpp"
#include "hipSYCL/runtime/executor.hpp"

using namespace hipsycl;
namespace fs = std::filesystem;


template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };


bool isin(std::string_view val, std::span<std::string> collection)
{
	return std::find(std::begin(collection), std::end(collection), val) != std::end(collection);
}


std::string upper(std::string_view s)
{
	std::string r;
	std::transform(begin(s), end(s), std::back_inserter(r), [](auto c){
		return std::toupper(c);
	});
	return r;
}


std::string lower(std::string_view s)
{
	std::string r;
	std::transform(begin(s), end(s), std::back_inserter(r), [](auto c){
		return std::tolower(c);
	});
	return r;
}


std::vector<std::string> split(std::string_view s, char sep)
{
	std::stringstream ss{std::string(s)};
	std::vector<std::string> r;
	std::string segment;
	while (std::getline(ss, segment, sep))
		r.push_back(segment);
	return r;
}


std::vector<std::byte> encode(std::string_view text, std::string_view encoding)
{
	std::vector<std::byte> r;
	for(auto c: text)
		r.push_back((std::byte)c);
	return r;
}


class uuid
{
public:
	static int uuid1()
	{
		return std::rand();
	}
};

void print(std::string msg)
{
	std::cout << msg << std::endl;
}

//--


void print_warning(std::string msg)
{
	std::cerr << "acpp warning: " << msg << std::endl;
}

void print_error(std::string msg)
{
	std::cerr << "acpp error: " << msg << std::endl;
}

class OptionNotSet : public std::runtime_error
{
public:
	OptionNotSet(std::string msg) :
		std::runtime_error(msg)
	{
	}
};

class hcf_node
{
public:
	using Value = std::variant<int, std::string>;

	static std::string str(Value v)
	{
		std::string r;
		std::visit(overloaded{
            [&](int arg) { r = std::to_string(arg); },
            [&](std::string arg) { r = arg; },
        }, v);
		return r;
	}

	hcf_node(std::string node_name, int nesting_level = 0) :
		node_name(node_name),
		nesting_level(nesting_level)
	{
	}

	~hcf_node()
	{
		for (auto& n : subnodes)
			delete n;
	}

	hcf_node* make_subnode(std::string name)
	{
		auto n = new hcf_node(name, nesting_level + 1);
		subnodes.push_back(n);
		return n;
	}

	void add_binary_attachment(int offset, int size)
	{
		auto n = make_subnode("__binary");
		n->values["start"] = offset;
		n->values["size"] = size;
	}

	std::string str() const
	{
		std::string result;
		std::string indent(' ', nesting_level);
		for(auto [key, val]: values)
			result += std::format("{}{}={}\n", indent, key, str(val));
		for(auto n : subnodes)
		{
			result += indent + "{." + n->node_name + "\n";
			result += n->str();
      		result += indent + "}." + n->node_name + "\n";
		}
		return result;
	}

	std::vector<hcf_node*> subnodes;
	std::map<std::string, Value> values;
	std::string node_name;
	int nesting_level;
};

template <>
struct std::formatter<hcf_node> : std::formatter<std::string> {
  auto format(hcf_node p, format_context& ctx) const {
    return formatter<string>::format(p.str(), ctx);
  }
};

class hcf_generator
{
public:
	hcf_generator():
		root("root")
	{
	}

	void attach_binary_content(hcf_node& node, std::vector<std::byte> content)
	{
		auto offset = binary_content.size();
		auto size = content.size();
		node.add_binary_attachment(offset, size);
		std::copy(begin(content), end(content), std::back_inserter(binary_content));
	}

	void attach_text_content(hcf_node& node, std::string content)
	{
		attach_binary_content(node, encode(content,"utf-8"));
	}

	std::string str() const
	{
		return root.str() + "__acpp_hcf_binary_appendix";
	}

	std::vector<std::byte> bytes() const
	{
		std::vector<std::byte> result = encode(str(), "utf-8");
		std::copy(begin(binary_content), end(binary_content), std::back_inserter(result));
		return result;
	}

	std::string escaped_bytes() const
	{
		std::string r;
		for(std::byte h: bytes())
			r += std::format("{:02x},", (int)h);
		return r;
	}

	hcf_node root;
	std::vector<std::byte> binary_content;
};

template <>
struct std::formatter<hcf_generator> : std::formatter<std::string> {
  auto format(hcf_generator p, format_context& ctx) const {
    return formatter<string>::format(p.str(), ctx);
  }
};

class integration_header
{
public:
	integration_header(std::string backend_name):
		backend(backend_name)
	{
		m_object_id = uuid::uuid1();
		hcf.root.values["object-id"] = std::format("{}", m_object_id);
		hcf.root.values["generator"] = "syclcc";
	}

	std::string str() const
	{
		using namespace fmt;
		std::string hcf_string = hcf.escaped_bytes();
		std::string header = fmt::format(R""""(
#ifndef ACPP_{capital_name}_INTEGRATION_HEADER
#define ACPP_{capital_name}_INTEGRATION_HEADER

static const std::size_t __acpp_local_{name}_hcf_object_id = {hcf_object_id}ull;
const unsigned char __acpp_hcf_object_{hcf_object_id} [] = {{ {hcf_binary} }};
ACPP_STATIC_HCF_REGISTRATION({hcf_object_id}ull, __acpp_hcf_object_{hcf_object_id}, {hcf_size})

#endif
		)"""", 
			"capital_name"_a = upper(backend),
			"name"_a = lower(backend),
			"hcf_object_id"_a = m_object_id,
			"hcf_size"_a = hcf.bytes().size(),
			"hcf_binary"_a = hcf_string
		);

		return header;
	}

	void write_header(fs::path filename)
	{
		std::ofstream f(filename);
		auto s = str();
		f.write(s.data(), s.size());
	}

	int m_object_id;
	hcf_generator hcf;
	std::string backend;
};

class config_db
{
public:
	// Scans the provided directory for json files
	config_db(std::vector<fs::path> config_file_dirs) :
		config_file_dirs(config_file_dirs)
	{
		for (auto& current_dir : config_file_dirs)
		{
			for (const auto & f : fs::directory_iterator(current_dir))
			{
				if(f.path().extension() == ".json")
				{
					//std::ifstream config_file(f);
				}
			}
		}
	}

	std::string get(std::string key)
	{
		if(!data.contains(key))
			throw std::runtime_error(std::format("Accessed missing key in config files: {}", key));
		return data.at(key);
	}

	std::string get_or_default(std::string key, std::string default_value)
	{
		if(data.contains(key))
			return data.at(key);
		return default_value;
	}

	std::vector<fs::path> config_file_dirs;
	std::map<std::string, std::string> data;
	bool is_loaded;
};

class option
{
public:
	option() = default;

	option(std::string_view commandline, std::string_view environment, std::string_view config_db, std::string_view description) :
		commandline(commandline), environment(environment), config_db(config_db), description(description)
	{
	}

	std::string commandline;
	std::string environment;
	std::string config_db;
	std::string description;
};

class acpp_config
{
public:
	/**
	Describes different representations of options:
	1.) the corresponding command line argument
	2.) the corresponding environment variable
	3.) the field in the config file.
	*/
	acpp_config(std::span<char*> args, std::map<std::string,std::string> envs)
	{
		m_options = {
			{"platform", option("--acpp-platform", "ACPP_PLATFORM", "default-platform",
				"  (deprecated) The platform that AdaptiveCpp should target. Valid values:\n"
				"* cuda: Target NVIDIA CUDA GPUs\n"
				"* rocm : Target AMD GPUs running on the ROCm platform\n"
				"* cpu : Target only CPUs\n")},
			{"clang", option("--acpp-clang", "ACPP_CLANG", "default-clang",
				"The path to the clang executable that should be used for compilation\n"
				"(Note: *must * be compatible with the clang version that the\n"
				"AdaptiveCpp clang plugin was compiled against!)\n")},
			{"nvcxx", option("--acpp-nvcxx", "ACPP_NVCXX", "default-nvcxx",
				"The path to the nvc++ executable that should be used for compilation\n"
				"with the cuda - nvcxx backend."
				)},
			{"cuda-path", option("--acpp-cuda-path", "ACPP_CUDA_PATH", "default-cuda-path",
				"The path to the CUDA toolkit installation directory")},
			{"rocm-path", option("--acpp-rocm-path", "ACPP_ROCM_PATH", "default-rocm-path",
				"The path to the ROCm installation directory")},
			{"gpu-arch", option("--acpp-gpu-arch", "ACPP_GPU_ARCH", "default-gpu-arch",
				 "(deprecated)The GPU architecture that should be targeted when compiling for GPUs.\n"
				"For CUDA, the architecture has the form sm_XX, e.g.sm_60 for Pascal.\n"
				"For ROCm, the architecture has the form gfxYYY, e.g.gfx900 for Vega 10, gfx906 for Vega 20.\n")},

    {"cpu-compiler", option("--acpp-cpu-cxx", "ACPP_CPU_CXX", "default-cpu-cxx",
		"  The compiler that should be used when targeting only CPUs.")},

    {"clang-include-path", option("--acpp-clang-include-path", "ACPP_CLANG_INCLUDE_PATH", "default-clang-include-path",
		"  The path to clang's internal include headers. Typically of the form $PREFIX/include/clang/<version>/include. Only required by ROCm.")},

    {"sequential-link-line", option("--acpp-sequential-link-line", "ACPP_SEQUENTIAL_LINK_LINE", "default-sequential-link-line",
		" The arguments passed to the linker for the sequential backend")},

    {"sequential-cxx-flags", option("--acpp-sequential-cxx-flags", "ACPP_SEQUENTIAL_CXX_FLAGS", "default-sequential-cxx-flags",
		" The arguments passed to the compiler to compile for the sequential backend")},

    {"omp-link-line", option("--acpp-omp-link-line", "ACPP_OMP_LINK_LINE", "default-omp-link-line",
		" The arguments passed to the linker for the OpenMP backend.")},

    {"omp-cxx-flags", option("--acpp-omp-cxx-flags", "ACPP_OMP_CXX_FLAGS", "default-omp-cxx-flags",
		" The arguments passed to the compiler to compile for the OpenMP backend")},

    {"rocm-link-line", option("--acpp-rocm-link-line", "ACPP_ROCM_LINK_LINE", "default-rocm-link-line",
		" The arguments passed to the linker for the ROCm backend.")},

    {"rocm-cxx-flags", option("--acpp-rocm-cxx-flags", "ACPP_ROCM_CXX_FLAGS", "default-rocm-cxx-flags",
		" The arguments passed to the compiler to compile for the ROCm backend")},

    {"cuda-link-line", option("--acpp-cuda-link-line", "ACPP_CUDA_LINK_LINE", "default-cuda-link-line",
		" The arguments passed to the linker for the CUDA backend.")},

    {"cuda-cxx-flags", option("--acpp-cuda-cxx-flags", "ACPP_CUDA_CXX_FLAGS", "default-cuda-cxx-flags",
		" The arguments passed to the compiler to compile for the CUDA backend")},

    {"config-file-dir", option("--acpp-config-file-dir", "ACPP_CONFIG_FILE_DIR", "default-config-file-dir",
		"  Select an alternative path for the config files containing the default AdaptiveCpp settings."
    	"It is normally not necessary for the user to change this setting. ")},

    {"targets", option("--acpp-targets", "ACPP_TARGETS", "default-targets", R""""(
  Specify backends and targets to compile for. Example: --acpp-targets='omp;hip:gfx900,gfx906'
    Available backends:
      * omp - OpenMP CPU backend
               Backend Flavors:
               - omp.library-only: Works with any OpenMP enabled CPU compiler.
                                   Uses Boost.Fiber for nd_range parallel_for support.
               - omp.accelerated: Uses clang as host compiler to enable compiler support
                                  for nd_range parallel_for (see --acpp-use-accelerated-cpu).
      * cuda - CUDA backend
               Requires specification of targets of the form sm_XY,
               e.g. sm_70 for Volta, sm_60 for Pascal
               Backend Flavors:
               - cuda.explicit-multipass: CUDA backend in explicit multipass mode
                                          (see --acpp-explicit-multipass)
               - cuda.integrated-multipass: Force CUDA backend to operate in integrated
                                           multipass mode.
      * cuda-nvcxx - CUDA backend with nvc++. Target specification is optional;
               if given requires the format ccXY.
      * hip  - HIP backend
               Requires specification of targets of the form gfxXYZ,
               e.g. gfx906 for Vega 20, gfx900 for Vega 10
               Backend Flavors:
               - hip.explicit-multipass: HIP backend in explicit multipass mode
                                         (see --acpp-explicit-multipass)
               - hip.integrated-multipass: Force HIP backend to operate in integrated
                                           multipass mode.
      * generic - use generic LLVM SSCP compilation flow, and JIT at runtime to target device
)"""")},

    {"stdpar-prefetch-mode", option("--acpp-stdpar-prefetch-mode", "ACPP_STDPAR_PREFETCH_MODE", "default-stdpar-prefetch-mode", R"""(
  AdaptiveCpp supports issuing automatic USM prefetch operations for allocations used inside offloaded C++ PSTL
    algorithms. This flags determines the strategy for submitting such prefetches.
    Supported values are:
      * always      - Prefetches every allocation used by every stdpar kernel
      * never       - Disables prefetching
      * after-sync  - Prefetch all allocations used by the first kernel submitted after each synchronization point.
                      (Prefetches running on non-idling queues can be expensive!)
      * first       - Prefetch allocations only the very first time they are used in a kernel
      * auto        - Let AdaptiveCpp decide (default))""")}
		};

		m_flags = {
			{"use-accelerated-cpu", option("--acpp-use-accelerated-cpu", "ACPP_USE_ACCELERATED_CPU", "default-use-accelerated-cpu",
			" If set, Clang is used for host compilation and explicit compiler support\n"
			"is enabled for accelerating the nd - range parallel_for on CPU.\n"
			"Uses continuation - based synchronization to execute all work - items\n"
			"of a work - group in a single thread, eliminating scheduling overhead\n"
			"and enabling enhanced vectorization opportunities compared to the fiber variant.\n")},
      {"is-dryrun", option("--acpp-dryrun", "ACPP_DRYRUN", "default-is-dryrun", R"""(
  If set, only shows compilation commands that would be executed,
  but does not actually execute it. )""")},
      {"is-explicit-multipass", option("--acpp-explicit-multipass", "ACPP_EXPLICIT_MULTIPASS", "default-is-explicit-multipass", R"""(
  If set, executes device passes as separate compiler invocation and lets AdaptiveCpp control embedding device
  images into the host binary. This allows targeting multiple backends simultaneously that might otherwise be
  incompatible. In this mode, source code level interoperability may not be supported in the host pass.
  For example, you cannot use the CUDA kernel launch syntax[i.e. kernel <<< ... >>> (...)] in this mode. )""")},
      {"should-save-temps", option("--acpp-save-temps", "ACPP_SAVE_TEMPS", "default-save-temps", 
	  	"  If set, do not delete temporary files created during compilation.")},
    	{"stdpar",  option("--acpp-stdpar", "ACPP_STDPAR", "default-is-stdpar",
		"  If set, enables SYCL offloading of C++ standard parallel algorithms.")},
      {"stdpar-system-usm", option("--acpp-stdpar-system-usm", "ACPP_STDPAR_SYSTEM_USM", "default-is-stdpar-system-usm", R"""(
  If set, assume availability of system-level unified shared memory where every pointer from regular
  malloc() is accessible on GPU. This disables automatic hijacking of memory allocations at the compiler
  level by AdaptiveCpp.)""")},
      {"stdpar-unconditional-offload", option("--acpp-stdpar-unconditional-offload", "ACPP_STDPAR_UNCONDITIONAL_OFFLOAD", "default-is-stdpar-unconditional-offload", R"""(
  Normally, heuristics are employed to determine whether algorithms should be offloaded.
  This particularly affects small problem sizes. If this flag is set, supported parallel STL
  algorithms will be offloaded unconditionally.)""")},
      {"is-export-all", option("--acpp-export-all", "ACPP_EXPORT_ALL", "default-export-all", R"""(
  (Experimental) Treat all functions implicitly as SYCL_EXTERNAL. Only supported with generic target.
  This currently only works with translation units that include the sycl.hpp header.)""")}
		};

		for (auto a : args)
			m_args.push_back(a);

		m_insufficient_cpp_standards = { "98", "03", "11", "14", "0x" };

		for (auto arg : m_args)
		{
			if (is_acpp_arg(arg))
				m_acpp_args.push_back(arg);
			else if (is_acpp_arg(upgrade_legacy_arg(arg)))
				m_acpp_args.push_back(arg);
			else
				m_forwarded_args.push_back(arg);
		}

		for (auto [envvar, envval] : envs)
		{
			if (is_acpp_envvar(envvar))
				m_acpp_environment_args[envvar] = envval;
			else if(is_acpp_envvar(upgrade_legacy_arg(envvar)))
				m_acpp_environment_args[upgrade_legacy_arg(envvar)] = envval;
		}

		//install_config_dir = config.acpp_installation_path / "etc" / "AdaptiveCpp";
		fs::path global_config_dir = "/etc/AdaptiveCpp";

		if(is_option_set_to_non_default_value("config-file-dir"))
		{

		}
	}

	bool is_acpp_arg(std::string arg)
	{
		std::set<std::string> accepted_vars;
		for (auto& opt : m_options)
			accepted_vars.insert(opt.second.commandline);
		for (auto& flag : m_flags)
			accepted_vars.insert(flag.second.commandline);
		for (auto accepted_arg : accepted_vars)
			if (std::string_view(arg).starts_with(accepted_arg + "=") || (arg == accepted_arg))
				return true;
		return false;
	}

	bool is_acpp_envvar(std::string varname)
	{
		std::set<std::string> accepted_vars;
		for (auto opt : m_options)
			accepted_vars.insert(opt.second.environment);
		for (auto flag : m_flags)
			accepted_vars.insert(flag.second.environment);
		return accepted_vars.contains(varname);
	}

	bool is_option_set_to_non_default_value(std::string)
	{
		return false;
	}

	void parse_compound_argument(std::string arg)
	{
	}

	void print_options()
	{
	}

	void print_flags()
	{
	}

	bool interpret_flag(std::string flag_value)
	{
		std::string v;
		std::transform(begin(flag_value), end(flag_value), std::back_inserter(v), 
			[](auto c) { return std::tolower(c); });
		if ((v == "0") || (v == "off") || (v == "false"))
			return false;
		return true;
	}

	bool is_flag_set(std::string flag_name)
	{
		auto flag = m_flags.at(flag_name);
		for (std::string_view arg : m_acpp_args)
		{
			if (arg == flag.commandline)
				return true;
			if (arg.starts_with(flag.commandline + "="))
				interpret_flag(split(arg, '=')[1]);
		}
		return false;
	}

	std::string upgrade_legacy_arg(std::string arg)
	{
		return arg;
	}

	bool use_accelerated_cpu()
	{
		return is_flag_set("accelerated");
	}



	bool has_optimization_flag()
	{
		return false;
	}

	bool is_pure_linking_stage()
	{
		return false;
	}

	bool is_explicit_multipass()
	{
		return false;
	}

	std::vector<std::string> m_insufficient_cpp_standards;
	std::map<std::string, option> m_options, m_flags;
	std::vector<std::string> m_args;
	std::vector<std::string> m_acpp_args;
	std::map<std::string, std::string> m_acpp_environment_args;
	std::vector<std::string> m_forwarded_args;
	std::vector<std::string> m_targets;
	std::string m_cxx_path;
	std::string m_clang_path;
};

void run_or_print(std::string command, bool print_only)
{
}

class Backend
{
};

class cuda_multipass_invocation
{
public:
};

class hip_multipass_invocation
{
public:
	constexpr static std::string_view unique_name = "hip.explicit-multipass";

	constexpr static bool is_integrated_multipass = false;

	static bool is_explicit_multipass()
	{
		return !is_integrated_multipass;
	}
};

class cuda_invocation
{
public:
	constexpr static std::string_view unique_name = "cuda.integrated-multipass";
};

class cuda_nvcxx_invocation
{
public:
};

class hip_invocation
{
public:
};

class omp_invocation
{
public:
};

class omp_accelerated_invocation : public Backend
{
public:
	omp_accelerated_invocation(acpp_config config, int targets)
	{
	}
};

/*
This is a workaround to have access to a backend
that can execute host tasks when compiling for GPUs.
It should be removed once we have non-OpenMP host backends
(e.g. TBB)
*/
class omp_sequential_invocation
{
public:
};

class llvm_sscp_invocation
{
public:
	constexpr static std::string_view unique_name = "sscp";

	constexpr static bool is_integrated_multipass = true;

	static bool is_explicit_multipass()
	{
		return !is_integrated_multipass;
	}

	llvm_sscp_invocation(acpp_config config, int targets)
	{
	}
};

class compiler
{
public:
	compiler(acpp_config config) :
		m_config(config)
	{
		m_clang_path = config.m_clang_path;
		m_is_explicit_multipass = config.is_explicit_multipass();

		if (isin("hip", m_config.m_targets) && isin("cuda", m_config.m_targets))
		{
			if (!m_is_explicit_multipass)
			{
				print_warning("CUDA and HIP cannot be targeted "
					"simultaneously in non-explicit multipass; enabling explicit "
					"multipass compilation.");
				m_is_explicit_multipass = true;
			}
		}

		for (auto backend : m_config.m_targets)
		{
			if (backend == "omp")
			{
				auto default_to_accelerated = [](acpp_config config)
					{
						return false;
					};
				//if (config.use_accelerated_cpu() || default_to_accelerated(config))
				//	m_backends.push_back(std::make_unique<omp_accelerated_invocation>(config, config.targets["omp"]));
			}
		}

		m_host_compiler = select_compiler();
	};

	std::string select_compiler()
	{
		std::string compiler_executable;
		int compiler_priority = 0;
		for (auto backend : m_config.m_targets)
		{
			//auto [cxx, priority] = backend.get_compiler_preference();
		}
		return {};
	}

	int run()
	{
		return -1;
	}

	acpp_config m_config;
	bool m_is_explicit_multipass;
	std::optional<std::string> m_clang_path;
	std::vector<std::unique_ptr<Backend>> m_backends;
	std::string m_host_compiler;
};


void print_version(acpp_config cfg)
{
}

void print_config(acpp_config cfg)
{
}

void print_usage(acpp_config config)
{
	print_version(config);
	print("Usage: acpp <options>\n");
	print("Options are:");
	config.print_options();
	config.print_flags();
	print("--acpp-version\n  Print AdaptiveCpp version and configuration\n");
	print("--help\n  Print this help message\n");
	print("\nAny other options will be forwarded to the compiler.");
	print("\nNote: Command line arguments take precedence over environment variables.");
}

#ifdef WIN32
#include <Windows.h>
#include <Processenv.h>

int main(int argc, char* argv[])
{
	auto free = [](LPTCH p) { FreeEnvironmentStrings(p); };
	auto env_block = std::unique_ptr<TCHAR, decltype(free)>{ GetEnvironmentStrings(), free };
	std::vector<char*> env_items;
	for (char* p_env = env_block.get(); *p_env != '\0'; ++p_env)
	{
		env_items.push_back(p_env);
		for (; *p_env != '\0'; ++p_env)
		{
		}
	}
	char** envp = env_items.data();
#else
int main(int argc, char** argv, char** envp)
{
#endif
	std::map<std::string, std::string> envs;
	for (auto env = envp; env != nullptr; env++)
	{
		auto keyval = split(std::string_view(*env), '=');
		envs[keyval.at(0)] = keyval.at(1);
	}

	fs::path filename = argv[0];
	if (filename == "syclcc")
		print_warning("syclcc is deprecated; please use acpp instead.");
	if (filename == "syclcc-clang")
		print_warning("syclcc-clang is deprecated; please use acpp instead.");

	auto args = std::span<char*>(argv + 1, argc - 1);

	try {
		auto config = acpp_config(args, envs);
		if (args.size() == 0)
		{
			print_usage(config);
			exit(-1);
		}
		for (std::string_view arg : args)
		{
			if (arg == "--help")
			{
				print_usage(config);
				exit(0);
			}
			else if ((arg == "--acpp-version") || (arg == "--opensycl-version") || (arg == "--hipsycl-version"))
			{
				print_version(config);
				print_config(config);
				exit(0);
			}
			else if (arg.starts_with("-fyscl"))
			{
				throw std::runtime_error(std::format(
					"The{} flag is a component from a different SYCL implementation. "
					"This flag is neither neither needed nor meaningful for AdaptiveCpp. "
					"Its use is unsupported.", arg));
			}
		}
		if (!config.is_pure_linking_stage())
			if (!config.has_optimization_flag())
				print_warning("No optimization flag was given, optimizations are "
					"disabled by default. Performance may be degraded. Compile with e.g. -O2/-O3 to "
					"enable optimizations.");

		auto c = compiler(config);
		exit(c.run());
	}
	catch (std::runtime_error& e)
	{
		print_error(std::format("fatal: {}", e.what()));
		exit(-1);
	}
	return 0;
}
