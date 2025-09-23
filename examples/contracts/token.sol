/**
 * @title ResearchToken
 * @dev A simple token implementation for research purposes
 * @author Deo Blockchain Research Team
 * @notice This is a simplified token implementation for educational use
 */

contract ResearchToken {
    // Token metadata
    string public name;
    string public symbol;
    uint8 public decimals;
    uint256 public totalSupply;
    
    // State variables
    mapping(address => uint256) public balances;
    mapping(address => mapping(address => uint256)) public allowances;
    
    // Events
    event Transfer(address indexed from, address indexed to, uint256 value);
    event Approval(address indexed owner, address indexed spender, uint256 value);
    event Mint(address indexed to, uint256 value);
    event Burn(address indexed from, uint256 value);
    
    // Modifiers
    modifier validAddress(address addr) {
        require(addr != address(0), "Invalid address");
        _;
    }
    
    modifier sufficientBalance(address addr, uint256 amount) {
        require(balances[addr] >= amount, "Insufficient balance");
        _;
    }
    
    // Constructor
    constructor(string memory _name, string memory _symbol, uint8 _decimals, uint256 _initialSupply) {
        name = _name;
        symbol = _symbol;
        decimals = _decimals;
        totalSupply = _initialSupply * 10**_decimals;
        balances[msg.sender] = totalSupply;
        emit Transfer(address(0), msg.sender, totalSupply);
    }
    
    /**
     * @dev Get the balance of an account
     * @param account The account to check
     * @return The balance of the account
     */
    function balanceOf(address account) public view returns (uint256) {
        return balances[account];
    }
    
    /**
     * @dev Transfer tokens to another address
     * @param to The address to transfer to
     * @param amount The amount to transfer
     * @return Success status
     */
    function transfer(address to, uint256 amount) public validAddress(to) sufficientBalance(msg.sender, amount) returns (bool) {
        balances[msg.sender] -= amount;
        balances[to] += amount;
        emit Transfer(msg.sender, to, amount);
        return true;
    }
    
    /**
     * @dev Transfer tokens from one address to another
     * @param from The address to transfer from
     * @param to The address to transfer to
     * @param amount The amount to transfer
     * @return Success status
     */
    function transferFrom(address from, address to, uint256 amount) public validAddress(to) sufficientBalance(from, amount) returns (bool) {
        require(allowances[from][msg.sender] >= amount, "Insufficient allowance");
        
        balances[from] -= amount;
        balances[to] += amount;
        allowances[from][msg.sender] -= amount;
        
        emit Transfer(from, to, amount);
        return true;
    }
    
    /**
     * @dev Approve an address to spend tokens
     * @param spender The address to approve
     * @param amount The amount to approve
     * @return Success status
     */
    function approve(address spender, uint256 amount) public validAddress(spender) returns (bool) {
        allowances[msg.sender][spender] = amount;
        emit Approval(msg.sender, spender, amount);
        return true;
    }
    
    /**
     * @dev Get the allowance of a spender
     * @param owner The owner of the tokens
     * @param spender The spender address
     * @return The allowance amount
     */
    function allowance(address owner, address spender) public view returns (uint256) {
        return allowances[owner][spender];
    }
    
    /**
     * @dev Mint new tokens (for research purposes)
     * @param to The address to mint tokens to
     * @param amount The amount to mint
     */
    function mint(address to, uint256 amount) public validAddress(to) {
        totalSupply += amount;
        balances[to] += amount;
        emit Mint(to, amount);
        emit Transfer(address(0), to, amount);
    }
    
    /**
     * @dev Burn tokens from an address
     * @param from The address to burn tokens from
     * @param amount The amount to burn
     */
    function burn(address from, uint256 amount) public sufficientBalance(from, amount) {
        balances[from] -= amount;
        totalSupply -= amount;
        emit Burn(from, amount);
        emit Transfer(from, address(0), amount);
    }
    
    /**
     * @dev Get token information
     * @return name, symbol, decimals, totalSupply
     */
    function getTokenInfo() public view returns (string memory, string memory, uint8, uint256) {
        return (name, symbol, decimals, totalSupply);
    }
}
