var eve = require('evejs');
var nodeModule = require('../build/Release/RecommenderModule');
var Promise = require('promise');

function RecommenderAgent(id, AIMid) {
	// super constructor
	eve.Agent.call(this, id);

	// extend with RPC
	this.rpc = this.loadModule('rpc', this.rpcFunctions);

	// connect to transports
	this.connect(eve.system.transports.getAll());

	// create objects
	this.AIMmodule = new nodeModule.RecommenderModule(AIMid + '');

}

RecommenderAgent.prototype = Object.create(eve.Agent.prototype);
RecommenderAgent.prototype.constructor = RecommenderAgent;

RecommenderAgent.prototype.rpcFunctions = {};

RecommenderAgent.prototype.rpcFunctions.train = function(params, sender) {
	// document to send is exactly the parameters for training
	var doc = params;
	// Write documents to ports
	this.AIMmodule.WriteDocument(doc);
}

RecommenderAgent.prototype.rpcFunctions.test = function(params, sender) {
	var termQuery = params;
	var module = this.AIMmodule;
	module.WriteTerm(termQuery);

	return new Promise(function(resolve, reject) {

		var recommendationCallback = function(msg) {
			console.log("Return msg: " + msg);
			resolve(msg);
		};
		// set callback for output from module
		module.RegReadRecommendation(recommendationCallback);
	});
}



module.exports = RecommenderAgent;
