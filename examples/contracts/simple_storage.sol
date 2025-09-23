/**
 * @title SimpleStorage
 * @dev A basic smart contract demonstrating state storage and retrieval
 * @author Deo Blockchain Research Team
 * @notice This contract is for educational and research purposes
 */

contract SimpleStorage {
    // State variables
    uint256 public storedData;
    address public owner;
    
    // Events
    event DataStored(uint256 indexed value, address indexed setter);
    event OwnershipTransferred(address indexed previousOwner, address indexed newOwner);
    
    // Modifiers
    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner can call this function");
        _;
    }
    
    // Constructor
    constructor() {
        owner = msg.sender;
        storedData = 0;
    }
    
    /**
     * @dev Store a value in the contract
     * @param x The value to store
     */
    function set(uint256 x) public {
        storedData = x;
        emit DataStored(x, msg.sender);
    }
    
    /**
     * @dev Retrieve the stored value
     * @return The stored value
     */
    function get() public view returns (uint256) {
        return storedData;
    }
    
    /**
     * @dev Increment the stored value by 1
     */
    function increment() public {
        storedData = storedData + 1;
        emit DataStored(storedData, msg.sender);
    }
    
    /**
     * @dev Decrement the stored value by 1
     */
    function decrement() public {
        require(storedData > 0, "Cannot decrement below zero");
        storedData = storedData - 1;
        emit DataStored(storedData, msg.sender);
    }
    
    /**
     * @dev Transfer ownership of the contract
     * @param newOwner The address of the new owner
     */
    function transferOwnership(address newOwner) public onlyOwner {
        require(newOwner != address(0), "New owner cannot be zero address");
        address previousOwner = owner;
        owner = newOwner;
        emit OwnershipTransferred(previousOwner, newOwner);
    }
    
    /**
     * @dev Get the current owner
     * @return The address of the current owner
     */
    function getOwner() public view returns (address) {
        return owner;
    }
}
