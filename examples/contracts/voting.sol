/**
 * @title SimpleVoting
 * @dev A basic voting contract for research and educational purposes
 * @author Deo Blockchain Research Team
 * @notice This contract demonstrates decentralized voting mechanisms
 */

contract SimpleVoting {
    // Voting session information
    struct VotingSession {
        string title;
        string description;
        uint256 startTime;
        uint256 endTime;
        bool isActive;
        mapping(bytes32 => uint256) votes;
        mapping(address => bool) hasVoted;
        bytes32[] candidates;
        uint256 totalVotes;
    }
    
    // State variables
    mapping(uint256 => VotingSession) public sessions;
    uint256 public sessionCount;
    address public owner;
    
    // Events
    event SessionCreated(uint256 indexed sessionId, string title, uint256 startTime, uint256 endTime);
    event VoteCast(uint256 indexed sessionId, bytes32 indexed candidate, address indexed voter);
    event SessionEnded(uint256 indexed sessionId, bytes32 winner, uint256 winningVotes);
    
    // Modifiers
    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner can perform this action");
        _;
    }
    
    modifier validSession(uint256 sessionId) {
        require(sessionId < sessionCount, "Invalid session ID");
        _;
    }
    
    modifier sessionActive(uint256 sessionId) {
        require(sessions[sessionId].isActive, "Session is not active");
        require(block.timestamp >= sessions[sessionId].startTime, "Session has not started");
        require(block.timestamp <= sessions[sessionId].endTime, "Session has ended");
        _;
    }
    
    // Constructor
    constructor() {
        owner = msg.sender;
        sessionCount = 0;
    }
    
    /**
     * @dev Create a new voting session
     * @param title The title of the voting session
     * @param description Description of the voting session
     * @param duration Duration of the session in seconds
     * @param candidateNames Array of candidate names
     * @return The session ID
     */
    function createSession(
        string memory title,
        string memory description,
        uint256 duration,
        string[] memory candidateNames
    ) public onlyOwner returns (uint256) {
        uint256 sessionId = sessionCount;
        VotingSession storage session = sessions[sessionId];
        
        session.title = title;
        session.description = description;
        session.startTime = block.timestamp;
        session.endTime = block.timestamp + duration;
        session.isActive = true;
        session.totalVotes = 0;
        
        // Add candidates
        for (uint256 i = 0; i < candidateNames.length; i++) {
            bytes32 candidate = keccak256(abi.encodePacked(candidateNames[i]));
            session.candidates.push(candidate);
            session.votes[candidate] = 0;
        }
        
        sessionCount++;
        emit SessionCreated(sessionId, title, session.startTime, session.endTime);
        return sessionId;
    }
    
    /**
     * @dev Cast a vote for a candidate
     * @param sessionId The ID of the voting session
     * @param candidate The candidate to vote for
     */
    function vote(uint256 sessionId, string memory candidate) public validSession(sessionId) sessionActive(sessionId) {
        VotingSession storage session = sessions[sessionId];
        require(!session.hasVoted[msg.sender], "Address has already voted");
        
        bytes32 candidateHash = keccak256(abi.encodePacked(candidate));
        require(session.votes[candidateHash] >= 0, "Invalid candidate"); // Check if candidate exists
        
        session.hasVoted[msg.sender] = true;
        session.votes[candidateHash]++;
        session.totalVotes++;
        
        emit VoteCast(sessionId, candidateHash, msg.sender);
    }
    
    /**
     * @dev Get the number of votes for a candidate
     * @param sessionId The ID of the voting session
     * @param candidate The candidate name
     * @return The number of votes
     */
    function getVotes(uint256 sessionId, string memory candidate) public view validSession(sessionId) returns (uint256) {
        bytes32 candidateHash = keccak256(abi.encodePacked(candidate));
        return sessions[sessionId].votes[candidateHash];
    }
    
    /**
     * @dev Check if an address has voted in a session
     * @param sessionId The ID of the voting session
     * @param voter The address to check
     * @return Whether the address has voted
     */
    function hasVoted(uint256 sessionId, address voter) public view validSession(sessionId) returns (bool) {
        return sessions[sessionId].hasVoted[voter];
    }
    
    /**
     * @dev Get session information
     * @param sessionId The ID of the voting session
     * @return title, description, startTime, endTime, isActive, totalVotes
     */
    function getSessionInfo(uint256 sessionId) public view validSession(sessionId) returns (
        string memory,
        string memory,
        uint256,
        uint256,
        bool,
        uint256
    ) {
        VotingSession storage session = sessions[sessionId];
        return (
            session.title,
            session.description,
            session.startTime,
            session.endTime,
            session.isActive,
            session.totalVotes
        );
    }
    
    /**
     * @dev End a voting session and determine the winner
     * @param sessionId The ID of the voting session
     */
    function endSession(uint256 sessionId) public onlyOwner validSession(sessionId) {
        VotingSession storage session = sessions[sessionId];
        require(session.isActive, "Session is already ended");
        require(block.timestamp > session.endTime, "Session has not ended yet");
        
        session.isActive = false;
        
        // Find the winner
        bytes32 winner = session.candidates[0];
        uint256 maxVotes = session.votes[winner];
        
        for (uint256 i = 1; i < session.candidates.length; i++) {
            bytes32 candidate = session.candidates[i];
            if (session.votes[candidate] > maxVotes) {
                winner = candidate;
                maxVotes = session.votes[candidate];
            }
        }
        
        emit SessionEnded(sessionId, winner, maxVotes);
    }
    
    /**
     * @dev Get the winner of a session
     * @param sessionId The ID of the voting session
     * @return The winner's candidate hash and vote count
     */
    function getWinner(uint256 sessionId) public view validSession(sessionId) returns (bytes32, uint256) {
        VotingSession storage session = sessions[sessionId];
        require(!session.isActive, "Session is still active");
        
        bytes32 winner = session.candidates[0];
        uint256 maxVotes = session.votes[winner];
        
        for (uint256 i = 1; i < session.candidates.length; i++) {
            bytes32 candidate = session.candidates[i];
            if (session.votes[candidate] > maxVotes) {
                winner = candidate;
                maxVotes = session.votes[candidate];
            }
        }
        
        return (winner, maxVotes);
    }
    
    /**
     * @dev Get the total number of sessions
     * @return The number of sessions
     */
    function getSessionCount() public view returns (uint256) {
        return sessionCount;
    }
}
