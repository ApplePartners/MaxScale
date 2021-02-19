#pragma once

#include <errno.h>
#include <string>
#include <set>
#include <vector>
#include <string>

#include <maxbase/ccdefs.hh>
#include <maxbase/string.hh>
#include <maxtest/mariadb_func.hh>

typedef std::set<std::string> StringSet;
class SharedData;

class Nodes
{
public:
    virtual ~Nodes() = default;

    const char* ip_private(int i = 0) const;

    /**
     * @brief Number of backend nodes
     */
    int N;

    bool verbose() const;


    /**
     * @brief mdbci_node_name
     * @param node
     * @return name of the node in MDBCI format
     */
    std::string mdbci_node_name(int node);

    // Simplified C++ version
    struct SshResult
    {
        int         rc {-1};
        std::string output;
    };
    SshResult ssh_output(const std::string& cmd, int node = 0, bool sudo = true);

    /**
     * @brief executes shell command on the node using ssh
     * @param index number of the node (index)
     * @param ssh command to execute
     * @param sudo if true the command is executed with root privelegues
     * @return exit code of the coomand
     */
    int ssh_node(int node, const std::string& ssh, bool sudo);
    int ssh_node_f(int node, bool sudo, const char* format, ...) mxb_attribute((format(printf, 4, 5)));

    /**
     * @brief Copy a local file to the Node i machine
     * @param src Source file on the local filesystem
     * @param dest Destination file on the remote file system
     * @param i Node index
     * @return exit code of the system command or 1 in case of i > N
     */
    int copy_to_node_legacy(const char* src, const char* dest, int i = 0);
    int copy_to_node(int i, const char* src, const char* dest);

    /**
     * @brief Copy a local file to the Node i machine
     * @param src Source file on the remote filesystem
     * @param dest Destination file on the local file system
     * @param i Node index
     * @return exit code of the system command or 1 in case of i > N
     */
    int copy_from_node_legacy(const char* src, const char* dest, int i);
    int copy_from_node(int i, const char* src, const char* dest);

    /**
     * @brief Check node via ssh and restart it if it is not resposible
     * @param node Node index
     * @return True if node is ok, false if start failed
     */
    bool check_nodes();

    /**
     * @brief read_basic_env Read IP, sshkey, etc - common parameters for all kinds of nodes
     * @return 0 in case of success
     */
    int read_basic_env();

    void write_env_vars();

protected:
    SharedData& m_shared;

    Nodes(const std::string& prefix, SharedData* shared, const std::string& network_config);

    const char* ip4(int i = 0) const;
    const char* ip6(int i = 0) const;

    const char* hostname(int i = 0) const;
    const char* access_user(int i = 0) const;
    const char* access_homedir(int i = 0) const;
    const char* access_sudo(int i = 0) const;
    const char* sshkey(int i = 0) const;

    const std::string& prefix() const;

    virtual bool setup();

private:

    class VMNode
    {
    public:
        VMNode(SharedData& shared, const std::string& name);
        VMNode(VMNode&& rhs);
        ~VMNode();

        bool init_ssh_master();

        enum class CmdPriv {NORMAL, SUDO};

        /**
         * Run a command on the VM, either through ssh or local terminal. No output.
         *
         * @param cmd Command string
         * @param priv Sudo or normal user
         * @return Return code
         */
        int run_cmd(const std::string& cmd, CmdPriv priv = CmdPriv::NORMAL);

        /**
         * Run a command on the VM, either through ssh or local terminal. Fetches output.
         *
         * @param cmd Command string
         * @param priv Sudo or normal user
         * @return Return code and command output
         */
        Nodes::SshResult
        run_cmd_output(const std::string& cmd, CmdPriv priv = CmdPriv::NORMAL);

        bool configure(const std::string& network_config);

        /**
         * Write node network info to environment variables. This is mainly needed by script-type tests.
         */
        void write_node_env_vars();

        std::string m_name;     /**< E.g. "node_001" */

        std::string m_ip4;          /**< IPv4-address */
        std::string m_ip6;          /**< IPv6-address */
        std::string m_private_ip;   /**< Private IP-address for AWS */
        std::string m_hostname;     /**< Hostname */

        std::string m_username; /**< Unix user name to access nodes via ssh */
        std::string m_homedir;  /**< Home directory of username */
        std::string m_sudo;     /**< empty or "sudo " */
        std::string m_sshkey;   /**< Path to ssh key */

    private:
        std::string get_nc_item(const std::string& item_name, const std::string& network_config);

        enum class NodeType {LOCAL, REMOTE};

        NodeType    m_type {NodeType::REMOTE};      /**< SSH only used on remote nodes */
        std::string m_ssh_cmd_p1;                   /**< Start of remote command string */
        FILE*       m_ssh_master_pipe {nullptr};    /**< Master ssh pipe. Kept open for ssh multiplex */
        SharedData& m_shared;
    };

    std::string m_prefix;                   /**< Name of backend setup (e.g. 'repl' or 'galera') */

    std::vector<VMNode> m_vms;

    std::string m_network_config;       /**< Contents of MDBCI network_config file */

    bool check_node_ssh(int node);

    /**
     * Calculate the number of nodes described in the network config file
     * @return Number of nodes
     */
    int get_N();
};
