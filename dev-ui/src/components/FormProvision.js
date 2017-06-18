import React from 'react'
import { Field, reduxForm } from 'redux-form'
import BasicField from './BasicField'

const validate = values => {
  const errors = {}
  if (!values.thingName) {
    errors.thingName = 'Required'
  }

  return errors
}

const FormProvision = (props) => {
  const { handleSubmit, invalid, pristine, submitting } = props
  return (
    <form onSubmit={handleSubmit}>
      <Field name='thingName' type='text' component={BasicField} label='thing name'/>
      <div className='control'>
        <button className={'button is-primary is-large' + (submitting ? ' is-loading' : '')} type='submit' disabled={invalid || pristine || submitting}>provision</button>
      </div>
    </form>
  )
}

export default reduxForm({
  form: 'FormProvision',
  validate
})(FormProvision)
