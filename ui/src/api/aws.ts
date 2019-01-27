import { Auth } from "aws-amplify";
import AWS from "aws-sdk";

class CCAWS {
  public async checkAuthExists(): Promise<boolean> {
    try { await Auth.currentAuthenticatedUser(); return true } catch(e) { return false } 
  }

  public async getLambdaClient(): Promise<AWS.Lambda> {
    const credentials = await Auth.currentCredentials();
    return new AWS.Lambda({
      credentials: Auth.essentialCredentials(credentials)
    });
  }

  public async invokeLambda(
    params: AWS.Lambda.InvocationRequest,
    callback?: (err: AWS.AWSError, data: AWS.Lambda.InvocationResponse) => void
  ): Promise<AWS.Request<AWS.Lambda.InvocationResponse, AWS.AWSError>> {
    // params.FunctionName = `${CCStack.StackName}-${params.FunctionName}`
    const client = await this.getLambdaClient();
    return client.invoke(params, callback);
  }
}

export const ccAWS = new CCAWS();
