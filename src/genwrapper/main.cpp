#include "Notify.h"
#include "NodeMap.h"
#include "TypeRegistry.h"
#include "RegistryBuilder.h"
#include "WrapperGenerator.h"
#include "Configuration.h"
#include "TypeNameUtils.h"

#include <iostream>
#include <fstream>
#include "config.h"

#ifdef ERROR
#undef ERROR
#endif

const std::string RELEASE_DATE = "Fri, 11 Jun 2010";

void replace_string(std::string &s, const std::string &from, const std::string &to)
{
    std::string::size_type p = s.find(from);
    while (p != std::string::npos)
    {
        s.replace(p, from.length(), to);
        p = s.find(from);
    }
}

void report_stats(const WrapperGenerator::Statistics &stats, std::ostream &os)
{
    if (stats.total_reflectors > 0)
    {
        os << "\n* a total of " << stats.total_reflectors << " reflectors were generated\n";
        os << "    " << stats.num_typedefs << " (" << (100 * stats.num_typedefs / stats.total_reflectors) << "%) are typedefs\n";
        os << "    " << stats.num_custom << " (" << (100 * stats.num_custom / stats.total_reflectors) << "%) are user-defined reflectors\n";
        
        if (stats.num_object_types > 0)
        {
            os << "    " << stats.num_object_types << " (" << (100 * stats.num_object_types / stats.total_reflectors) << "%) are object types, ";
            os << stats.num_abstract_object_types << " (" << (100 * stats.num_abstract_object_types / stats.num_object_types) << "%) of which are abstract\n";
        }
        
        if (stats.num_value_types > 0)
        {
            os << "    " << stats.num_value_types << " (" << (100 * stats.num_value_types / stats.total_reflectors) << "%) are value types, subdivided as follows:\n";
            os << "        " << stats.num_enums << " (" << (100 * stats.num_enums / stats.num_value_types) << "%) are enumerations\n";
            os << "        " << stats.num_stdcontainers << " (" << (100 * stats.num_stdcontainers / stats.num_value_types) << "%) are STL containers\n";
        }
    }

    if (stats.total_methods > 0)
    {
        os << "\n* a total of " << stats.total_methods << " methods were wrapped\n";
        os << "    " << stats.total_constructors << " (" << (100 * stats.total_constructors / stats.total_methods) << "%) are constructors\n";
        os << "    " << stats.total_static << " (" << (100 * stats.total_static / stats.total_methods) << "%) are static\n";
        os << "    " << stats.total_protected << " (" << (100 * stats.total_protected / stats.total_methods) << "%) are protected\n";
    }

    if (stats.total_properties > 0)
    {
        os << "\n* a total of " << stats.total_properties << " properties were defined\n";
        os << "    " << stats.num_simple_properties << " (" << (100 * stats.num_simple_properties / stats.total_properties) << "%) are simple properties\n";
        os << "    " << stats.num_array_properties << " (" << (100 * stats.num_array_properties / stats.total_properties) << "%) are array properties\n";
        os << "    " << stats.num_indexed_properties << " (" << (100 * stats.num_indexed_properties / stats.total_properties) << "%) are indexed properties\n";
        os << "    " << stats.num_public_properties << " (" << (100 * stats.num_public_properties / stats.total_properties) << "%) are public properties\n";
    }
}

void show_help(std::ostream &os)
{
    os << "OpenSceneGraph Introspection Wrapper Generator\n";
    os << "Version " << PACKAGE_VERSION << " - " << RELEASE_DATE << "\n";
    os << "\n";
    os << "Usage: genwrapper -h | -?\n";
    os << "       genwrapper -d path_to_osg [doxy_dir]\n";
    os << "       genwrapper [-c config_file] [-v level] [-p] [-m] [-l]\n";
    os << "                  [doxy_dir] output_dir\n";
    os << "\nARGUMENTS:\n";
    os << "path_to_osg    Root directory of OpenSceneGraph\n";
    os << "\n";
    os << "doxy_dir       Base directory that contains (or that is the destination for)\n";
    os << "               XML files generated by Doxygen. Default is the system-provided\n";
    os << "               temporary directory, if any.\n";
    os << "\n";
    os << "output_dir     Base output directory. Wrapper files will be placed in\n";
    os << "               `{output_dir}/src/osgWrappers/introspection/'.\n";
    os << "\n";
    os << "\n";
    os << "OPTIONS:\n";
    os << "  -h or -?        Show this help screen\n";
    os << "  -d              Generate a Doxyfile\n";
    os << "  -c config-file  Load the specified configuration file (this option can\n";
    os << "                  be repeated multiple times)\n";
    os << "  -v level        Specify the level of verbosity, where level can be\n";
    os << "                  one of: QUIET, ERROR, WARNING, NOTICE, INFO, DEBUG\n";
    os << "  -p              Enable creation of VisualStudio 6 project files\n";
    os << "  -m              Enable creation of GNU makefiles\n";
    os << "  -l              Create a list of added, modified and removed files in each\n";
    os << "                  target directory\n";
}

int generate_doxyfile(const std::string& templateFile, const std::string &appdir, const std::string &osg_dir, const std::string &doxy_dir)
{
    std::ifstream ifs(templateFile.c_str());
    if (!ifs.is_open())
    {
        ifs.clear();
        ifs.open((appdir + templateFile).c_str());
        if (!ifs.is_open())
        {
            std::cerr << "genwrapper: could not open Doxygen.template" << std::endl;
            return 1;
        }
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        replace_string(line, "$(INPUT_DIR)", osg_dir);
        replace_string(line, "$(OUTPUT_DIR)", doxy_dir);
        std::cout << line << "\n";
    }

    return 0;
}

int main(int argc, char *argv[])
{
    std::string appdir = PathNameUtils::getDirectoryName(argv[0]);

    enum ActionType
    {
        BUILD_WRAPPERS,
        GENERATE_DOXYFILE
    };

    ActionType action = BUILD_WRAPPERS;
    bool create_lists = false;
    std::string templateFile = "Doxyfile.template";

    typedef std::vector<std::string> StringList;

    StringList cfgfiles;
    StringList args;

    if (argc > 1)
    {
        if (strncmp(argv[1], "--help", strlen("--help")) == 0)
        {
            show_help(std::cout); 
            return 2;
        }
        else if (strncmp(argv[1], "--version", strlen("--version")) == 0)
        {
            std::cout << PACKAGE_VERSION << std::endl;
            return 2;
        }
    }


    for (int i=1; i<argc; ++i)
    {
        int more_args = argc - i - 1;

        if (argv[i][0] == '-' && argv[i][1] != 0)
        {
            char opt = argv[i][1];
            switch (opt)
            {
                case '?':
                case 'h': 
                    show_help(std::cout); 
                    return 2;

                case 'd':
                    action = GENERATE_DOXYFILE;
                    break;

                case 't':
                    templateFile = argv[i+1];
                    ++i;
                    break;

                case 'c':
                    if (more_args > 0)
                    {
                        cfgfiles.push_back(argv[i+1]);
                        ++i;
                    }
                    break;

                case 'v':
                    if (more_args > 0)
                    {
                        std::string level = argv[i+1];
                        if (level == "QUIET") Notify::setLevel(Notify::QUIET);
                        if (level == "ERROR") Notify::setLevel(Notify::ERROR);
                        if (level == "WARNING") Notify::setLevel(Notify::WARNING);
                        if (level == "NOTICE") Notify::setLevel(Notify::NOTICE);
                        if (level == "INFO") Notify::setLevel(Notify::INFO);
                        if (level == "DEBUG") Notify::setLevel(Notify::DEBUG);
                        ++i;
                    }
                    break;

                case 'l':
                    create_lists = true;
                    break;

                default: 
                    std::cerr << "genwrapper: unrecognized option `-" << opt << "'" << std::endl;
                    return 1;
            };
        }
        else
            args.push_back(argv[i]);
    }

    bool invalid_args = false;

    const char *tmpdir;
#ifdef WIN32
    tmpdir = getenv("TEMP");
#else
    tmpdir = P_tmpdir;
#endif

    if (!tmpdir)
    {
        std::cerr << "genwrapper: could not get the name of the default temporary directory. Please specify the `doxy_dir' argument manually" << std::endl;
        return 1;
    }

    std::string doxy_dir = PathNameUtils::consolidateDelimiters(tmpdir, true);
    std::string osg_dir;
    std::string output_dir;

    if (action == GENERATE_DOXYFILE)
    {
        if (args.size() == 1 || args.size() == 2)
        {
            osg_dir = PathNameUtils::consolidateDelimiters(args.front(), true);
            if (args.size() == 2)
                doxy_dir = PathNameUtils::consolidateDelimiters(args.back(), true);


            return generate_doxyfile(templateFile, appdir, osg_dir, doxy_dir);
        }
        else
            invalid_args = true;
    }

    if (action == BUILD_WRAPPERS)
    {
        if (args.size() == 1 || args.size() == 2)
        {
            output_dir = args.back();
            if (args.size() == 2)
                doxy_dir = PathNameUtils::consolidateDelimiters(args.front(), true);
        }
        else
            invalid_args = true;
    }

    if (invalid_args)
    {
        std::cerr << "genwrapper: invalid arguments" << std::endl;
        return 1;
    }

    doxy_dir = doxy_dir + "xml/";

    try
    {
        Configuration cfg;

        if (!cfgfiles.empty())
        {
            Notify::notice("loading configuration files");
            for (StringList::const_iterator i=cfgfiles.begin(); i!=cfgfiles.end(); ++i)
            {
                if (!cfg.load(*i))
                    Notify::error("could not load configuration file `" + *i + "'");
            }
        }
        else
        {
            if (!cfg.load("./genwrapper.conf"))
            {
                if (!cfg.load(appdir + "genwrapper.conf"))
                    Notify::error("no configuration files were specified, and the default configuration file `genwrapper.conf' could not be found");
            }
            Notify::notice("default configuration file loaded succesfully");
        }

        NodeMap nodemap(doxy_dir);

        Notify::notice("loading XML index file");

        if (!nodemap.loadIndex())
            Notify::error("could not load the XML index file");

        TypeRegistry registry(cfg);
        RegistryBuilder builder(nodemap, registry, cfg);

        Notify::notice("building type registry");        
        builder.build();

        Notify::notice("consolidating type registry");
        registry.consolidate();

        WrapperGenerator generator(registry, output_dir, "src/osgWrappers/introspection", appdir, create_lists, cfg);

        Notify::notice("generating wrappers");
        generator.generate();

        std::cout << "done.\n\n";
        std::cout << "REPORT:\n";
        report_stats(generator.getStatistics(), std::cout);
    }
    catch(const std::exception &e)
    {
        if (e.what()[0] != 0)
            std::cerr << "* ERROR: " << e.what() << std::endl;
        return 1;
    }
    catch(...)
    {
        std::cerr << "* UNKNOWN EXCEPTION" << std::endl;
        return 1;
    }

    return 0;
}
