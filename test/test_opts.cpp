#include <c4/std/string.hpp>
#include <c4/conf/conf.hpp>
#include <c4/fs/fs.hpp>
#include <c4/span.hpp>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <string>

C4_SUPPRESS_WARNING_GCC_CLANG_PUSH
C4_SUPPRESS_WARNING_GCC_CLANG("-Wold-style-cast")

namespace c4 {
namespace conf {

void test_opts(std::vector<std::string> const& input_args,
               std::vector<std::string> const& filtered_args,
               cspan<ParsedOpt> expected_args,
               yml::Tree const& expected_tree,
               cspan<ConfigActionSpec> specs={},
               yml::Tree const* reftree_=nullptr);

void to_args(std::vector<std::string> const& stringvec, std::vector<char*> *args);
std::vector<char*> to_args(std::vector<std::string> const& stringvec)
{
    std::vector<char*> args;
    to_args(stringvec, &args);
    return args;
}


const csubstr reftree = R"(
key0:
  key0val0:
    - key0val0val0
    - key0val0val1
    - key0val0val2
  key0val1:
    - key0val1val0
    - key0val1val1
    - key0val1val2
key1:
  key1val0:
    - key1val0val0
    - key1val0val1
    - key1val0val2
  key1val1:
    - key1val1val0
    - key1val1val1
    - key1val1val2
)";

void action1(yml::Tree &t, csubstr) { t["key0"]["key0val0"][0].set_val("key0val0val0-action1"); }
void action2(yml::Tree &t, csubstr) { t["key0"]["key0val0"][0].set_val("key0val0val0-action2"); }

const ConfigActionSpec specs_buf[] = {
    spec_for<ConfigAction::set_node>("-n", "--node"),
    spec_for<ConfigAction::load_file>("-f", "--file"),
    spec_for<ConfigAction::load_dir>("-d", "--dir"),
    {ConfigAction::callback, action1, csubstr("-a1"), csubstr("--action1" ), csubstr{}                 , csubstr("action 1")},
    {ConfigAction::callback, action2, csubstr("-a2"), csubstr("--action2" ), csubstr{}                 , csubstr("action 2")},
    {ConfigAction::callback, action2, csubstr("-o" ), csubstr("--optional"), csubstr("[<optionalval>]"), csubstr{}},
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

size_t call_with_args(std::initializer_list<std::string> il)
{
    std::vector<std::string> args_buf(il);
    std::vector<char*> args = to_args(args_buf);
    int argc = (int)args.size();
    char ** argv = args.data();
    return parse_opts(&argc, &argv, specs_buf, C4_COUNTOF(specs_buf), nullptr, 0);
}

TEST_CASE("opts.missing_args_are_flagged")
{
    CHECK_EQ(call_with_args({"-n"  /*missing*/}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-n"  /*missing*/, "-n", "notmissing"}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-n", "notmissing", "-n"  /*missing*/}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-n", "notmissing", "-n", "notmissing"}), 2u);
    CHECK_EQ(call_with_args({"-f"  /*missing*/}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-f"  /*missing*/, "-f", "notmissing"}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-f", "notmissing", "-f"  /*missing*/}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-f", "notmissing", "-f", "notmissing"}), 2u);
    CHECK_EQ(call_with_args({"-d"  /*missing*/}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-d"  /*missing*/, "-d", "notmissing"}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-d", "notmissing", "-d"  /*missing*/}), (size_t)argerror);
    CHECK_EQ(call_with_args({"-d", "notmissing", "-d", "notmissing"}), 2u);
}

TEST_CASE("opts.optional_args_are_not_mandatory")
{
    CHECK_EQ(call_with_args({"-o"  /*missing*/}), 1u);
    CHECK_EQ(call_with_args({"-o"  /*missing*/, "-o", "notmissing"}), 2u);
    CHECK_EQ(call_with_args({"-o", "notmissing", "-o"  /*missing*/}), 2u);
    CHECK_EQ(call_with_args({"-o", "notmissing", "-o", "notmissing"}), 2u);
    CHECK_EQ(call_with_args({"-o"  /*missing*/, "-o"   /*missing*/}), 2u);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("opts.args_are_not_changed_when_given_insufficient_output_buffer")
{
    std::vector<std::string> originalbuf = {"-n", "key0=val0", "-b", "b0", "-n", "key1=val1", "-c", "c0", "c1"};
    std::vector<std::string> expectedbuf = {                   "-b", "b0",                    "-c", "c0", "c1"};
    std::vector<char *> original;
    std::vector<char *> expected;
    to_args(originalbuf, &original);
    to_args(expectedbuf, &expected);
    REQUIRE_GT(originalbuf.size(), expectedbuf.size());
    size_t num_opts = 2u;
    std::vector<ParsedOpt> output;
    std::vector<char *> actual = original;
    int argc = (int)actual.size();
    char ** argv = actual.data();
    size_t ret;
    ret = parse_opts(&argc, &argv,
                     specs_buf, C4_COUNTOF(specs_buf),
                     output.data(), output.size());
    REQUIRE_NE(ret, (size_t)argerror);
    CHECK_EQ(ret, num_opts);
    CHECK_EQ((size_t)argc, actual.size());
    CHECK_EQ(argv, actual.data());
    CHECK_EQ(actual, original);
    output.resize(ret);
    ret = parse_opts(&argc, &argv,
                     specs_buf, C4_COUNTOF(specs_buf),
                     output.data(), output.size());
    CHECK_EQ(ret, num_opts);
    CHECK_EQ((size_t)argc, expected.size());
    actual.resize((size_t)argc);
    CHECK_EQ(*argv, original[2]); // the first non-filtered option
    for(size_t i = 0; i < (size_t)argc; ++i)
    {
        INFO("i=", i);
        CHECK_EQ(to_csubstr(actual[i]), to_csubstr(expected[i]));
    }
}

TEST_CASE("opts.empty_is_ok")
{
    cspan<ParsedOpt> expected_args = {};
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    test_opts({"-a", "-b", "b0", "-c", "c0", "c1"},
              {"-a", "-b", "b0", "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST_CASE("opts.set_node")
{
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::set_node, csubstr("key1.key1val0[1]"), csubstr("here it is"), {}},
        {ConfigAction::set_node, csubstr("key1.key1val1"   ), csubstr("now this is a scalar"), {}},
        {ConfigAction::set_node, csubstr("key1.key1val0[1]"), csubstr("here it is overrided"), {}},
    };
    expected_tree["key1"]["key1val0"][1].set_val("here it is");
    expected_tree["key1"]["key1val1"].clear_children();
    expected_tree["key1"]["key1val1"].set_type(yml::KEYVAL);
    expected_tree["key1"]["key1val1"].set_val("now this is a scalar");
    expected_tree["key1"]["key1val0"][1].set_val("here it is overrided");
    // quotes in the value
    test_opts({"-a", "-n", "key1.key1val0[1]='here it is'", "-b", "b0", "--node", "key1.key1val1=\"now this is a scalar\"", "-c", "c0", "c1", "-n", "key1.key1val0[1]='here it is overrided'"},
              {"-a",                                        "-b", "b0",                                                     "-c", "c0", "c1"},
              expected_args,
              expected_tree);
    // quotes in the arg
    test_opts({"-a", "-n", "'key1.key1val0[1]=here it is'", "-b", "b0", "--node", "\"key1.key1val1=now this is a scalar\"", "-c", "c0", "c1", "-n", "'key1.key1val0[1]=here it is overrided'"},
              {"-a",                                        "-b", "b0",                                                     "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST_CASE("opts.add_node_to_seq")
{
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::set_node, csubstr("key1.key1val0[5]"), csubstr("here you go with a new one"), {}},
    };
    expected_tree["key1"]["key1val0"].append_child().set_val({}); // 3
    expected_tree["key1"]["key1val0"].append_child().set_val({}); // 4
    expected_tree["key1"]["key1val0"].append_child().set_val("here you go with a new one");
    // quotes in the value
    test_opts({"-a", "-n", "key1.key1val0[5]='here you go with a new one'", "-b", "b0"},
              {"-a",                                                        "-b", "b0"},
              expected_args,
              expected_tree);
    // quotes in the arg
    test_opts({"-a", "-n", "'key1.key1val0[5]=here you go with a new one'", "-b", "b0"},
              {"-a",                                                        "-b", "b0"},
              expected_args,
              expected_tree);
}

TEST_CASE("opts.add_node_to_map")
{
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::set_node, csubstr("key1.key1val2"), csubstr("here you go with a new one"), {}},
    };
    expected_tree["key1"]["key1val2"] = "here you go with a new one";
    // quotes in the value
    test_opts({"-a", "-n", "key1.key1val2='here you go with a new one'", "-b", "b0"},
              {"-a",                                                     "-b", "b0"},
              expected_args,
              expected_tree);
    // quotes in the arg
    test_opts({"-a", "-n", "'key1.key1val2=here you go with a new one'", "-b", "b0"},
              {"-a",                                                     "-b", "b0"},
              expected_args,
              expected_tree);
}



TEST_CASE("opts.set_node_with_nonscalars")
{
    csubstr k1k1v01_ = "{nothing: really, actually: something}";
    csubstr k1k1v1 = "[more, items, like, this, are, appended]";
    csubstr k1k1v01 = "{Jacquesson: [741, 742], Gosset: Grande Reserve}";
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::set_node, csubstr("key1.key1val0[1]"), k1k1v01_, {}},
        {ConfigAction::set_node, csubstr("key1.key1val1"), k1k1v1, {}},
        {ConfigAction::set_node, csubstr("key1.key1val0[1]"), k1k1v01, {}},
    };
    expected_tree["key1"]["key1val0"][1].change_type(yml::MAP);
    expected_tree["key1"]["key1val0"][1]["nothing"] = "really";
    expected_tree["key1"]["key1val0"][1]["actually"] = "something";
    expected_tree["key1"]["key1val1"][3] = "more";
    expected_tree["key1"]["key1val1"][4] = "items";
    expected_tree["key1"]["key1val1"][5] = "like";
    expected_tree["key1"]["key1val1"][6] = "this";
    expected_tree["key1"]["key1val1"][7] = "are";
    expected_tree["key1"]["key1val1"][8] = "appended";
    expected_tree["key1"]["key1val0"][1]["Jacquesson"] |= yml::SEQ;
    expected_tree["key1"]["key1val0"][1]["Jacquesson"][0] = "741";
    expected_tree["key1"]["key1val0"][1]["Jacquesson"][1] = "742";
    expected_tree["key1"]["key1val0"][1]["Gosset"] = "Grande Reserve";
    // quotes in the value
    test_opts({"-a", "-n", "key1.key1val0[1]='{nothing: really, actually: something}'", "-b", "b0", "--node", "key1.key1val1=\"[more, items, like, this, are, appended]\"", "-c", "c0", "c1", "-n", "key1.key1val0[1]='{Jacquesson: [741, 742], Gosset: Grande Reserve}'"},
              {"-a",                                                                    "-b", "b0",                                                                         "-c", "c0", "c1"},
              expected_args,
              expected_tree);
    // quotes in the arg
    test_opts({"-a", "-n", "'key1.key1val0[1]={nothing: really, actually: something}'", "-b", "b0", "--node", "\"key1.key1val1=[more, items, like, this, are, appended]\"", "-c", "c0", "c1", "-n", "'key1.key1val0[1]={Jacquesson: [741, 742], Gosset: Grande Reserve}'"},
              {"-a",                                                                    "-b", "b0",                                                                         "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

struct case1files
{
    case1files()
    {
        auto mkdir = [](const char *d){
            if(fs::dir_exists(d))
                C4_CHECK(fs::rmtree(d) == 0);
            C4_CHECK(!fs::dir_exists(d));
            C4_CHECK(fs::mkdir(d) == 0);
        };
        mkdir("somedir");
        mkdir("somedir_to_node");
        mkdir("somedir_to_key0");
        mkdir("somedir_to_key1");
        fs::file_put_contents("somedir/file0", csubstr("{key0: {key0val0: now replaced as a scalar}}"));
        fs::file_put_contents("somedir/file2", csubstr("{key0: {key0val0: NOW replaced as a scalar v2}}"));
        fs::file_put_contents("somedir/file3", csubstr("{key1: {key1val0: THIS one too v2}}"));
        fs::file_put_contents("somedir/file1", csubstr("{key1: {key1val0: this one too}}"));
        // these are all equivalent:
        fs::file_put_contents("somedir_to_node/file0", csubstr("{key0val0: now replaced as a scalar}"));
        fs::file_put_contents("somedir_to_node/file2", csubstr("{key0val0: NOW replaced as a scalar v2}"));
        fs::file_put_contents("somedir_to_key0/file0", csubstr("{key0val0: now replaced as a scalar}"));
        fs::file_put_contents("somedir_to_key0/file2", csubstr("{key0val0: NOW replaced as a scalar v2}"));
        fs::file_put_contents("somedir_to_node/file3", csubstr("{key1val0: THIS one too v2}"));
        fs::file_put_contents("somedir_to_node/file1", csubstr("{key1val0: this one too}"));
        fs::file_put_contents("somedir_to_key1/file3", csubstr("{key1val0: THIS one too v2}"));
        fs::file_put_contents("somedir_to_key1/file1", csubstr("{key1val0: this one too}"));
    }
    ~case1files()
    {
        C4_CHECK(fs::rmtree("somedir") == 0);
        C4_CHECK(fs::rmtree("somedir_to_node") == 0);
        C4_CHECK(fs::rmtree("somedir_to_key0") == 0);
        C4_CHECK(fs::rmtree("somedir_to_key1") == 0);
    }
    void transform1(yml::Tree *tree)
    {
        (*tree)["key0"]["key0val0"].clear_children();
        (*tree)["key0"]["key0val0"].set_type(yml::KEYVAL);
        (*tree)["key0"]["key0val0"].set_val("now replaced as a scalar");
        (*tree)["key1"]["key1val0"].clear_children();
        (*tree)["key1"]["key1val0"].set_type(yml::KEYVAL);
        (*tree)["key1"]["key1val0"].set_val("this one too");
    }
    void transform2(yml::Tree *tree)
    {
        (*tree)["key0"]["key0val0"].clear_children();
        (*tree)["key0"]["key0val0"].set_type(yml::KEYVAL);
        (*tree)["key0"]["key0val0"].set_val("NOW replaced as a scalar v2");
        (*tree)["key1"]["key1val0"].clear_children();
        (*tree)["key1"]["key1val0"].set_type(yml::KEYVAL);
        (*tree)["key1"]["key1val0"].set_val("THIS one too v2");
    }
};

TEST_CASE("opts.load_file")
{
    case1files setup;
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::load_file, {}, csubstr("somedir/file0"), {}},
        {ConfigAction::load_file, {}, csubstr("somedir/file1"), {}},
    };
    setup.transform1(&expected_tree);
    test_opts({"-a", "-f", "somedir/file0", "-b", "b0", "--file", "somedir/file1", "-c", "c0", "c1"},
              {"-a",                        "-b", "b0",                            "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST_CASE("opts.load_file_to_node")
{
    case1files setup;
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::load_file, csubstr("key0"), csubstr("somedir_to_node/file0"), {}},
        {ConfigAction::load_file, csubstr("key1"), csubstr("somedir_to_node/file1"), {}},
    };
    setup.transform1(&expected_tree);
    test_opts({"-a", "-f", "key0=somedir_to_node/file0", "-b", "b0", "--file", "key1=somedir_to_node/file1", "-c", "c0", "c1"},
              {"-a",                                     "-b", "b0",                                         "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST_CASE("opts.load_dir")
{
    case1files setup;
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::load_dir, {}, csubstr("somedir"), {}},
    };
    setup.transform2(&expected_tree);
    test_opts({"-a", "-d", "somedir", "-b", "b0", "-c", "c0", "c1"},
              {"-a",                  "-b", "b0", "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST_CASE("opts.load_dir_to_node")
{
    case1files setup;
    yml::Tree expected_tree = yml::parse_in_arena(reftree);
    ParsedOpt expected_args[] = {
        {ConfigAction::load_dir, csubstr("key0"), csubstr("somedir_to_key0"), {}},
        {ConfigAction::load_dir, csubstr("key1"), csubstr("somedir_to_key1"), {}},
    };
    setup.transform2(&expected_tree);
    test_opts({"-a", "-d", "key0=somedir_to_key0", "-b", "b0", "-d", "key1=somedir_to_key1", "-c", "c0", "c1"},
              {"-a",                               "-b", "b0",                               "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


TEST_CASE("opts.github4")
{
    const yml::Tree tree = yml::parse_in_arena("test: {test1: false, test2: false}");

    auto action1 = [](yml::Tree &t, csubstr) { t["test"]["test1"] = "true"; };
    auto action2 = [](yml::Tree &t, csubstr) { t["test"]["test2"] = "true"; };
    auto action3 = [](yml::Tree &t, csubstr arg) { t["test"]["test3"] = arg; };

    const ConfigActionSpec specs[] = {
        {ConfigAction::callback, action1, csubstr("-t1"), csubstr("--action1"), csubstr{}                 , csubstr("action 1")},
        {ConfigAction::callback, action2, csubstr("-t2"), csubstr("--action2"), csubstr{}                 , csubstr("action 2")},
        {ConfigAction::callback, action3, csubstr("-t3"), csubstr("--action3"), csubstr("[<optionalval>]"), csubstr{}},
    };

    SUBCASE("no options")
    {
        test_opts({}, {}, {}, tree, specs, &tree);
    }
    SUBCASE("t1")
    {
        ParsedOpt expected_args[] = {ParsedOpt{ConfigAction::callback, {}, {}, action1}};
        const yml::Tree expected_tree = yml::parse_in_arena("test: {test1: true, test2: false}");
        test_opts({"-t1"}, {}, expected_args, expected_tree, specs, &tree);
    }
    SUBCASE("t2")
    {
        ParsedOpt expected_args[] = {ParsedOpt{ConfigAction::callback, {}, {}, action2}};
        const yml::Tree expected_tree = yml::parse_in_arena("test: {test1: false, test2: true}");
        test_opts({"-t2"}, {}, expected_args, expected_tree, specs, &tree);
    }
    SUBCASE("t1, t2")
    {
        ParsedOpt expected_args[] = {
            ParsedOpt{ConfigAction::callback, {}, {}, action1},
            ParsedOpt{ConfigAction::callback, {}, {}, action2}
        };
        const yml::Tree expected_tree = yml::parse_in_arena("test: {test1: true, test2: true}");
        test_opts({"-t1", "-t2"}, {}, expected_args, expected_tree, specs, &tree);
    }
    SUBCASE("t2, t1")
    {
        ParsedOpt expected_args[] = {
            ParsedOpt{ConfigAction::callback, {}, {}, action2},
            ParsedOpt{ConfigAction::callback, {}, {}, action1}
        };
        const yml::Tree expected_tree = yml::parse_in_arena("test: {test1: true, test2: true}");
        test_opts({"-t2", "-t1"}, {}, expected_args, expected_tree, specs, &tree);
    }
    SUBCASE("t1, t2, t3 foo")
    {
        ParsedOpt expected_args[] = {
            ParsedOpt{ConfigAction::callback, {}, {}, action1},
            ParsedOpt{ConfigAction::callback, {}, {}, action2},
            ParsedOpt{ConfigAction::callback, {}, csubstr{"foo"}, action3},
        };
        const yml::Tree expected_tree = yml::parse_in_arena("test: {test1: true, test2: true, test3: foo}");
        test_opts({"-t1", "-t2", "-t3", "foo"}, {}, expected_args, expected_tree, specs, &tree);
    }
    SUBCASE("t1, t2, t3 foo")
    {
        ParsedOpt expected_args[] = {
            ParsedOpt{ConfigAction::callback, {}, {}, action1},
            ParsedOpt{ConfigAction::callback, {}, csubstr{"foo"}, action3},
            ParsedOpt{ConfigAction::callback, {}, {}, action2},
        };
        const yml::Tree expected_tree = yml::parse_in_arena("test: {test1: true, test2: true, test3: foo}");
        test_opts({"-t1", "-t3", "foo", "-t2"}, {}, expected_args, expected_tree, specs, &tree);
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void to_args(std::vector<std::string> const& stringvec, std::vector<char*> *args)
{
    C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wcast-qual")
    args->resize(stringvec.size());
    for(size_t i = 0; i < stringvec.size(); ++i)
        (*args)[i] = (char *)&(stringvec[i][0]);
    C4_SUPPRESS_WARNING_GCC_CLANG_POP
}

size_t parse_opts(int *argc, char ***argv, c4::span<ParsedOpt> *opt_args, cspan<ConfigActionSpec> specs={})
{
    if(specs.empty())
        specs = specs_buf;
    size_t required = parse_opts(argc, argv,
                                 specs.data(), specs.size(),
                                 opt_args ? opt_args->data() : nullptr, opt_args ? opt_args->size() : 0);
    if(required == argerror)
        return argerror;
    if(opt_args && required <= opt_args->size())
        *opt_args = opt_args->first(required);
    return required;
}

void test_opts(std::vector<std::string> const& input_args,
               std::vector<std::string> const& filtered_args,
               cspan<ParsedOpt> expected_args,
               yml::Tree const& expected_tree,
               cspan<ConfigActionSpec> specs,
               yml::Tree const* reftree_)
{
    std::vector<char*> input_args_ptr, filtered_args_ptr, input_args_ptr_orig;
    int argc;
    char **argv;
    auto reset_args = [&]{
        to_args(input_args, &input_args_ptr);
        to_args(filtered_args, &filtered_args_ptr);
        argc = (int) input_args_ptr.size();
        argv = input_args_ptr.data();
        input_args_ptr_orig = input_args_ptr;
    };
    auto check_input_args = [&]{
        CHECK_EQ(argc, (int)input_args.size());
        for(int iarg = 0; iarg < argc; ++iarg)
            CHECK_EQ(to_csubstr(input_args_ptr[(size_t)iarg]), to_csubstr(input_args_ptr_orig[(size_t)iarg]));
    };
    // must accept nullptr
    reset_args();
    size_t ret = parse_opts(&argc, &argv, nullptr, specs);
    REQUIRE_NE(ret, (size_t)argerror);
    CHECK_EQ(ret, expected_args.size());
    check_input_args();
    // must deal with insufficient buffer size
    reset_args();
    std::vector<ParsedOpt> buf;
    buf.resize(expected_args.size() / 2);
    span<ParsedOpt> buf_out = {buf.data(), buf.size()};
    ret = parse_opts(&argc, &argv, &buf_out, specs);
    REQUIRE_NE(ret, (size_t)argerror);
    CHECK_EQ(ret, expected_args.size());
    CHECK_EQ(argc, (int)input_args.size());
    CHECK_EQ(buf_out.size(), buf.size());
    CHECK_EQ(buf_out.data(), buf.data());
    check_input_args();
    // must deal with sufficient buffer size
    reset_args();
    buf.resize(expected_args.size());
    buf_out = {buf.data(), buf.size()};
    ret = parse_opts(&argc, &argv, &buf_out, specs);
    REQUIRE_NE(ret, (size_t)argerror);
    CHECK_EQ(ret, expected_args.size());
    CHECK_EQ(argc, (int)filtered_args.size());
    REQUIRE_EQ(buf_out.size(), expected_args.size());
    CHECK_EQ(buf_out.data(), buf.data());
    for(int iarg = 0; iarg < argc; ++iarg)
    {
        INFO("iarg=", iarg);
        CHECK_EQ(to_csubstr(input_args_ptr[(size_t)iarg]), to_csubstr(filtered_args_ptr[(size_t)iarg]));
    }
    for(size_t iarg = 0; iarg < expected_args.size(); ++iarg)
    {
        INFO("iarg=", iarg);
        CHECK_EQ(buf_out[iarg].action, expected_args[iarg].action);
        CHECK_EQ(buf_out[iarg].target, expected_args[iarg].target);
        CHECK_EQ(buf_out[iarg].payload, expected_args[iarg].payload);
    }
    //
    if(expected_args.size())
    {
        yml::Tree output = reftree_ ? *reftree_ : yml::parse_in_arena(reftree);
        Workspace ws(&output);
        ws.apply_opts(buf_out.data(), buf_out.size());
        CHECK_EQ(yml::emitrs_yaml<std::string>(output), yml::emitrs_yaml<std::string>(expected_tree));
    }
}

} // namespace conf
} // namespace c4

C4_SUPPRESS_WARNING_GCC_CLANG_POP
