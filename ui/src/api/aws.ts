import { Auth } from 'aws-amplify'
import AWS from 'aws-sdk'
import { InvocationResponse } from 'aws-sdk/clients/lambda'

class CCAWS {
  public async checkAuthExists(): Promise<boolean> {
    try {
      await Auth.currentAuthenticatedUser()
      return true
    } catch (e) {
      return false
    }
  }

  public async getLambdaClient(): Promise<AWS.Lambda> {
    const credentials = await Auth.currentCredentials()
    return new AWS.Lambda({
      credentials: Auth.essentialCredentials(credentials),
    })
  }

  /**
   * Wrap lambda invoke in a sane async interface.
   */
  public async invokeLambda(
    params: AWS.Lambda.InvocationRequest,
    callback?: (err: AWS.AWSError, data: AWS.Lambda.InvocationResponse) => void
  ): Promise<InvocationResponse> {
    const client = await this.getLambdaClient()

    return new Promise((resolve, reject) => {
      client.invoke(params, (err, data) => {
        // error handling
        if (err) return reject(err)
        if (data && data.FunctionError) return reject(data)
        const anyData = data as any
        if (anyData && anyData.errorMessage) return reject(anyData.errorMessage)

        if (callback) callback(err, data)

        return resolve(data)
      })
    })
  }
}

export const ccAWS = new CCAWS()
